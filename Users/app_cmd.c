#include "app_cmd.h"
#include "blue.h"
#include "drv_flash_storage.h"
#include "event_manager.h"
#include "stm32f1xx_hal.h"
#include "key.h"
#include "app_boot_param.h"
#include "app_pika_runtime.h"
#include "drv_comm.h"
#include "drv_bt_config.h"
#include "drv_ymodem.h"
#include "drv_mem.h"
#include "beep.h"
#include "usbd_cdc_interface.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include <string.h>
#include <stdio.h>

#define USB_CMD_LINE_MAX        96U
#define BT_CMD_LINE_MAX         96U
#define USB_CMD_FW_UPDATE       "ymodem update fmware"

#define YMODEM_OK_BEEP_HZ1      880U
#define YMODEM_OK_BEEP_MS1      50U
#define YMODEM_OK_BEEP_HZ2      1175U
#define YMODEM_OK_BEEP_MS2      70U
#define YMODEM_FAIL_BEEP_HZ1    440U
#define YMODEM_FAIL_BEEP_MS1    100U
#define YMODEM_FAIL_BEEP_HZ2    330U
#define YMODEM_FAIL_BEEP_MS2    120U

static volatile uint8_t s_ymodem_active;
static volatile uint8_t s_upload_paused;
static volatile uint8_t s_bt_at_busy;        /* 正在通过蓝牙模块执行 AT 命令 */
static uint32_t s_bt_at_guard_until;        /* 忽略模块回显 AT 命令的截止时间 */
static char s_bt_at_last_line[BT_CMD_LINE_MAX];
static uint8_t s_usb_line[USB_CMD_LINE_MAX];
static uint8_t s_usb_idx;
static uint8_t s_bt_line[BT_CMD_LINE_MAX];
static uint8_t s_bt_idx;

/* 蓝牙模块属性本地缓存（监控上报用）*/
#define BT_NAME_MAX_LEN     21U     /* 最长 20 字节 + null */
#define BT_ADV_DATA_MAX_LEN 45U     /* 22 字节 Hex 字符串 + null */
static char s_bt_name[BT_NAME_MAX_LEN];
static char s_bt_adv_data[BT_ADV_DATA_MAX_LEN];

const char *AppBt_GetName(void)
{
    return s_bt_name;
}

const char *AppBt_GetAdvData(void)
{
    return s_bt_adv_data;
}

void AppBt_SetName(const char *name)
{
    if (name != NULL)
    {
        (void)strncpy(s_bt_name, name, BT_NAME_MAX_LEN - 1U);
        s_bt_name[BT_NAME_MAX_LEN - 1U] = 0U;
    }
}

void AppBt_SetAdvData(const char *hex)
{
    if (hex != NULL)
    {
        (void)strncpy(s_bt_adv_data, hex, BT_ADV_DATA_MAX_LEN - 1U);
        s_bt_adv_data[BT_ADV_DATA_MAX_LEN - 1U] = 0U;
    }
}

void AppMonitor_SetUploadPaused(uint8_t paused)
{
    s_upload_paused = (paused != 0U) ? 1U : 0U;
}

uint8_t AppMonitor_IsUploadPaused(void)
{
    return s_upload_paused;
}

uint8_t AppCmd_IsYmodemActive(void)
{
    return s_ymodem_active;
}

static void app_ymodem_play_result_beep(uint8_t ok)
{
    if (ok != 0U)
    {
        beep_play_melody("880,1175", 50);
    }
    else
    {
        beep_play_melody("440,330", 100);
    }
}

static void app_run_ymodem(DrvCommPort_t port)
{
    DrvComm_t comm;
    uint8_t ok;

    if (DrvComm_Bind(&comm, port) != 0)
    {
        return;
    }
    set_event_disable("monitor_event");
    s_ymodem_active = 1U;
    AppMonitor_SetUploadPaused(1U);
    if (port == DRV_COMM_UART5)
    {
        DrvBtRing_SetPassthrough(1U);
    }

    ok = (DrvYmodem_ReceiveToRam(&comm) == 0) ? 1U : 0U;
    if (port == DRV_COMM_UART5)
    {
        DrvBtRing_Flush();
    }
    if (ok != 0U)
    {
        uint32_t flen = DrvMem_GetFileLength();
        (void)usb_printf("\r\nYMODEM OK, size=%lu\r\n", (unsigned long)flen);
        if (AppPika_LoadBytecode(DrvMem_GetFileBuffer(), flen) != 0)
        {
            (void)usb_printf("\r\n[Pika] invalid bytecode (.py.o)\r\n");
        }
    }
    else
    {
        (void)usb_printf("\r\nYMODEM FAIL (timeout/abort)\r\n");
    }
    app_ymodem_play_result_beep(ok);
    AppMonitor_SetUploadPaused(0U);
    set_event_enable("monitor_event");
  //  if (port == DRV_COMM_UART5)
    {
        DrvBtRing_SetPassthrough(0U);
    }
    s_ymodem_active = 0U;
}

void busDataparsing(_AGREEMENT *frame, void (*port_transerf_data)(void *data, uint16_t length))
{
    (void)port_transerf_data;
    if (frame == NULL)
    {
        return;
    }
    switch (frame->index)
    {
        case 0xB6:
        case 0xB9:
            if (AppPika_GetState() == APP_PIKA_STATE_RUNNING)
            {
                (void)AppPika_Stop();
            }
            else if (AppPika_HasBytecode() != 0U)
            {
                start_py = true;
                /* AppPika_Start() 由主循环统一调用，避免在 ISR 上下文中启动 VM */
            }
            break;
        case 0xBE:
            set_event_disable("monitor_event");
            break;
        case 0xBA:
            set_event_enable("monitor_event");
            break;
        default:
            break;
    }
}

/*
 * 向蓝牙模块发送 AT 命令，等待返回，通过 USB printf 输出结果
 * 如果命令设置了蓝牙名字或广播数据，同步更新本地缓存用于监控上报
 * 因为等待蓝牙模块响应会短暂阻塞，所以只适合非实时关键路径
 */
static uint8_t s_last_cmd_ok;    /* 最近一次 AT 命令是否成功 */

static uint8_t app_bt_line_is_ok(const char *val)
{
    const char *p = val;

    if (val == NULL)
    {
        return 0U;
    }
    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }
    return ((p[0] == 'O') && (p[1] == 'K') &&
            ((p[2] == '\0') || (p[2] == '\r') || (p[2] == '\n'))) ? 1U : 0U;
}

static uint8_t app_bt_line_is_error(const char *val)
{
    const char *p = val;

    if (val == NULL)
    {
        return 0U;
    }
    while ((*p == ' ') || (*p == '\t'))
    {
        p++;
    }
    return ((p[0] == 'E') && (p[1] == 'R') && (p[2] == 'R') &&
            (p[3] == 'O') && (p[4] == 'R') &&
            ((p[5] == '\0') || (p[5] == '\r') || (p[5] == '\n'))) ? 1U : 0U;
}

#define BT_AT_TIMEOUT_MS        2500U
#define BT_AT_ECHO_GUARD_MS     400U

static void app_bt_drain_uart5_echo(void)
{
    uint32_t t0 = HAL_GetTick();

    while ((HAL_GetTick() - t0) < BT_AT_ECHO_GUARD_MS)
    {
        DrvBtRing_Flush();
        delay_ms(10U);
    }
}

static void app_bt_parse_chunk(const char *chunk)
{
    char line[96];
    const char *p;
    const char *eol;

    if (chunk == NULL)
    {
        return;
    }

    p = chunk;
    while (*p != '\0')
    {
        eol = strchr(p, '\n');
        if (eol != NULL)
        {
            size_t n = (size_t)(eol - p);
            if (n >= sizeof(line))
            {
                n = sizeof(line) - 1U;
            }
            memcpy(line, p, n);
            line[n] = '\0';
            p = eol + 1;
        }
        else
        {
            (void)strncpy(line, p, sizeof(line) - 1U);
            line[sizeof(line) - 1U] = '\0';
            p += strlen(p);
        }

        if ((line[0] != '\0') && (line[strlen(line) - 1U] == '\r'))
        {
            line[strlen(line) - 1U] = '\0';
        }

        if (strstr(line, "+NAME:") != NULL)
        {
            AppBt_SetName(line + 6);
        }
        if (strstr(line, "+RESE:") != NULL)
        {
            AppBt_SetAdvData(line + 6);
        }
    }
}

static uint8_t app_bt_chunk_done(const char *chunk, uint8_t *out_ok)
{
    char line[96];
    const char *p;
    const char *eol;

    if ((chunk == NULL) || (out_ok == NULL))
    {
        return 0U;
    }

    p = chunk;
    while (*p != '\0')
    {
        eol = strchr(p, '\n');
        if (eol != NULL)
        {
            size_t n = (size_t)(eol - p);
            if (n >= sizeof(line))
            {
                n = sizeof(line) - 1U;
            }
            memcpy(line, p, n);
            line[n] = '\0';
            p = eol + 1;
        }
        else
        {
            (void)strncpy(line, p, sizeof(line) - 1U);
            line[sizeof(line) - 1U] = '\0';
            p += strlen(p);
        }

        if ((line[0] != '\0') && (line[strlen(line) - 1U] == '\r'))
        {
            line[strlen(line) - 1U] = '\0';
        }

        if (app_bt_line_is_ok(line) != 0U)
        {
            *out_ok = 1U;
            return 1U;
        }
        if (app_bt_line_is_error(line) != 0U)
        {
            *out_ok = 0U;
            return 1U;
        }
    }
    return 0U;
}

static uint8_t app_bt_exchange_at(UART_HandleTypeDef *huart, DEV_BLUE *blue,
                                  const uint8_t *tx, uint16_t tx_len, uint8_t *out_ok)
{
    uint32_t t0;

    if ((huart == NULL) || (blue == NULL) || (tx == NULL) || (out_ok == NULL))
    {
        return 0U;
    }

    *out_ok = 0U;
    blue->is_resh_flag = false;
    DrvBtRing_Flush();

    if (huart->gState != HAL_UART_STATE_READY)
    {
        huart->gState = HAL_UART_STATE_READY;
    }
    if (huart->RxState != HAL_UART_STATE_READY)
    {
        huart->RxState = HAL_UART_STATE_READY;
    }
    huart->Lock = HAL_UNLOCKED;

    if (HAL_UART_Transmit(huart, (uint8_t *)tx, tx_len, 500U) != HAL_OK)
    {
        return 0U;
    }

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < BT_AT_TIMEOUT_MS)
    {
        if (blue->is_resh_flag != false)
        {
            blue->is_resh_flag = false;
            (void)usb_printf("[BT] << %s\r\n", blue->at_cmd_bufer);
            app_bt_parse_chunk(blue->at_cmd_bufer);
            if (app_bt_chunk_done(blue->at_cmd_bufer, out_ok) != 0U)
            {
                return 1U;
            }
        }
        delay_ms(5U);
    }
    return 0U;
}

static void app_send_bt_at_cmd(const char *line)
{
    UART_HandleTypeDef *huart = getusartHandle(5);
    DEV_BLUE *blue = (DEV_BLUE *)getHubBase(PORT_BLUE);
    uint8_t tx_buf[128];
    uint16_t len;
    const char *val;

    if ((huart == NULL) || (blue == NULL))
    {
        (void)usb_printf("\r\n[BT] UART5/blue not ready\r\n");
        return;
    }

    /* 构造 AT 命令（line 已经不含末尾 \r\n）*/
    len = (uint16_t)snprintf((char *)tx_buf, sizeof(tx_buf), "%s\r\n", line);
    if (len >= sizeof(tx_buf))
    {
        (void)usb_printf("\r\n[BT] cmd too long\r\n");
        return;
    }

    s_bt_at_busy = 1U;
    s_last_cmd_ok = 0U;
    (void)strncpy(s_bt_at_last_line, line, sizeof(s_bt_at_last_line) - 1U);
    s_bt_at_last_line[sizeof(s_bt_at_last_line) - 1U] = '\0';

    (void)usb_printf("\r\n[BT] >> %s\r\n", line);

    if (app_bt_exchange_at(huart, blue, tx_buf, len, &s_last_cmd_ok) == 0U)
    {
        (void)usb_printf("[BT] command failed or timeout\r\n");
    }
    (void)usb_printf("\r\n");

    /*
     * 如果设置类命令成功，解析 line 中的参数值同步到本地缓存
     * 因为查询命令已经通过 "+NAME:" / "+RESE:" 响应更新了，这里只处理设置命令
     */
    if (s_last_cmd_ok != 0U)
    {
        /* AT+NAME=XXX */
        if (strstr(line, "AT+NAME=") != NULL)
        {
            val = line + 8;
            AppBt_SetName(val);
        }
        /* AT+RESEOFF — 关闭广播数据（须在 AT+RESE= 之前判断） */
        else if (strcmp(line, "AT+RESEOFF") == 0)
        {
            AppBt_SetAdvData("");
        }
        /* AT+RESE=XXX */
        else if (strstr(line, "AT+RESE=") != NULL)
        {
            val = line + 8;
            AppBt_SetAdvData(val);
        }
    }

    s_bt_at_guard_until = HAL_GetTick() + BT_AT_ECHO_GUARD_MS;
    app_bt_drain_uart5_echo();
    s_bt_at_busy = 0U;
}

static void app_handle_cmd_line(const char *line, DrvCommPort_t port)
{
    if (line == NULL)
    {
        return;
    }
    if (strcmp(line, USB_CMD_FW_UPDATE) == 0)
    {
        (void)usb_printf("\r\nFirmware update: reboot to Boot...\r\n");
        delay_ms(50);
        AppBoot_RequestFirmwareUpdate();
        return;
    }
    if (strcmp(line, "ymodem") == 0)
    {
        set_event_disable("monitor_event");
        AppMonitor_SetUploadPaused(1U);
        app_run_ymodem(port);
        return;
    }

    /* 以 "AT+" 开头的命令：仅 USB 下发；UART5 回显由 PollBt 读入，不可再次转发 */
    if ((line[0] == 'A' || line[0] == 'a') &&
        (line[1] == 'T' || line[1] == 't') &&
        line[2] == '+')
    {
        if (port != DRV_COMM_USB)
        {
            return;
        }
        if ((HAL_GetTick() < s_bt_at_guard_until) &&
            (strcmp(line, s_bt_at_last_line) == 0))
        {
            return;
        }
        app_send_bt_at_cmd(line);
        return;
    }

    if (port == DRV_COMM_USB)
    {
        (void)usb_printf("\r\nCMD:%s\r\n", line);
    }
}

static void app_feed_byte(uint8_t b, uint8_t *line, uint8_t *idx, uint8_t max_len, DrvCommPort_t port)
{
    if (b == '\r' || b == '\n')
    {
        if (*idx > 0U)
        {
            line[*idx] = 0U;
            app_handle_cmd_line((char *)line, port);
            *idx = 0U;
        }
        return;
    }
    if (*idx < (max_len - 1U))
    {
        line[(*idx)++] = b;
    }
}

void AppCmd_Init(void)
{
    DrvUsbRing_Init();
    DrvBtRing_Init();
    s_ymodem_active = 0U;
    s_upload_paused = 0U;
    s_bt_at_busy = 0U;
    s_bt_at_guard_until = 0U;
    s_bt_at_last_line[0] = '\0';
    s_usb_idx = 0U;
    s_bt_idx = 0U;

    /* 初始化蓝牙模块属性缓存 */
    (void)strncpy(s_bt_name, DRV_BT_NAME, BT_NAME_MAX_LEN - 1U);
    s_bt_name[BT_NAME_MAX_LEN - 1U] = 0U;
    s_bt_adv_data[0] = 0U;
}

void AppCmd_SyncBtFromModule(void)
{
    /*
     * 监控 JSON 的 btName/btAdvData 来自 MCU 缓存，不是自动从模块读的。
     * 已初始化过的板子开机后查询一次，避免监控里一直显示默认名。
     */
    if (DrvFlashStorage_IsBtConfigured() == 0)
    {
        return;
    }

    app_send_bt_at_cmd("AT+NAME?");
    app_send_bt_at_cmd("AT+RESE?");
}

void AppCmd_PollUsb(void)
{
    uint8_t b;
    if (s_ymodem_active != 0U)
    {
        return;
    }
    while (DrvUsbRing_ReadByte(&b, 0U))
    {
        app_feed_byte(b, s_usb_line, &s_usb_idx, USB_CMD_LINE_MAX, DRV_COMM_USB);
    }
}

void AppCmd_PollBt(void)
{
    uint8_t b;
    if ((s_ymodem_active != 0U) || (s_bt_at_busy != 0U))
    {
        return;
    }
    while (DrvBtRing_ReadByte(&b, 0U))
    {
        app_feed_byte(b, s_bt_line, &s_bt_idx, BT_CMD_LINE_MAX, DRV_COMM_UART5);
    }
}

/* Task5: cmd_poll 事件回调（TIM6 ISR，阈值 1 tick）。VM 阻塞期间命令轮询保持实时。
 * 内部沿用 AppCmd_PollUsb/PollBt 原有 s_ymodem_active/s_bt_at_busy 门控，行为不变。 */
void app_cmd_poll_callback(void *arg)
{
    (void)arg;
    AppCmd_PollUsb();
    AppCmd_PollBt();
}
