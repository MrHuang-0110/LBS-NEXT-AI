/*
 * at.c — 蓝牙模块 AT 桥（自 Users/app_cmd.c 拆出，Task 9）
 *
 * 迁移：app_bt_line_is_ok/is_error、app_bt_drain_uart5_echo、app_bt_parse_chunk、
 * app_bt_chunk_done、app_bt_exchange_at、app_send_bt_at_cmd（改名 at_send_line）、
 * s_bt_name/s_bt_adv_data 缓存与 AppBt_*（改名 At_*）。
 * 与 blue_at_cmd（blue.c）的合并在 Task 17 处理。
 */
#include "at.h"
#include "blue.h"
#include "device_pool.h"
#include "drv_comm.h"
#include "drv_bt_config.h"
#include "drv_flash_storage.h"
#include "usbd_cdc_interface.h"
#include "usart.h"
#include "delay.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>

#define BT_CMD_LINE_MAX         96U
#define BT_AT_TIMEOUT_MS        2500U
#define BT_AT_ECHO_GUARD_MS     400U

/* 蓝牙模块属性本地缓存（监控上报用）*/
#define BT_NAME_MAX_LEN     21U     /* 最长 20 字节 + null */
#define BT_ADV_DATA_MAX_LEN 45U     /* 22 字节 Hex 字符串 + null */

static char s_bt_name[BT_NAME_MAX_LEN];
static char s_bt_adv_data[BT_ADV_DATA_MAX_LEN];
static volatile uint8_t s_bt_at_busy;        /* 正在通过蓝牙模块执行 AT 命令 */
static uint32_t s_bt_at_guard_until;        /* 忽略模块回显 AT 命令的截止时间 */
static char s_bt_at_last_line[BT_CMD_LINE_MAX];
static uint8_t s_last_cmd_ok;               /* 最近一次 AT 命令是否成功 */

const char *At_GetName(void)
{
    return s_bt_name;
}

const char *At_GetAdvData(void)
{
    return s_bt_adv_data;
}

void At_SetName(const char *name)
{
    if (name != NULL)
    {
        (void)strncpy(s_bt_name, name, BT_NAME_MAX_LEN - 1U);
        s_bt_name[BT_NAME_MAX_LEN - 1U] = 0U;
    }
}

void At_SetAdvData(const char *hex)
{
    if (hex != NULL)
    {
        (void)strncpy(s_bt_adv_data, hex, BT_ADV_DATA_MAX_LEN - 1U);
        s_bt_adv_data[BT_ADV_DATA_MAX_LEN - 1U] = 0U;
    }
}

uint8_t At_IsBusy(void)
{
    return s_bt_at_busy;
}

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
            At_SetName(line + 6);
        }
        if (strstr(line, "+RESE:") != NULL)
        {
            At_SetAdvData(line + 6);
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

static void at_send_line(const char *line)
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
            At_SetName(val);
        }
        /* AT+RESEOFF — 关闭广播数据（须在 AT+RESE= 之前判断） */
        else if (strcmp(line, "AT+RESEOFF") == 0)
        {
            At_SetAdvData("");
        }
        /* AT+RESE=XXX */
        else if (strstr(line, "AT+RESE=") != NULL)
        {
            val = line + 8;
            At_SetAdvData(val);
        }
    }

    s_bt_at_guard_until = HAL_GetTick() + BT_AT_ECHO_GUARD_MS;
    app_bt_drain_uart5_echo();
    s_bt_at_busy = 0U;
}

/* 对外入口：过滤 UART5 回显的 AT 行后发送（USB 端口限制由业务 line handler 判断） */
void At_SendLine(const char *line)
{
    if (line == NULL)
    {
        return;
    }
    if ((HAL_GetTick() < s_bt_at_guard_until) &&
        (strcmp(line, s_bt_at_last_line) == 0))
    {
        return;
    }
    at_send_line(line);
}

void At_Init(void)
{
    s_bt_at_busy = 0U;
    s_bt_at_guard_until = 0U;
    s_bt_at_last_line[0] = '\0';

    /* 初始化蓝牙模块属性缓存 */
    (void)strncpy(s_bt_name, DRV_BT_NAME, BT_NAME_MAX_LEN - 1U);
    s_bt_name[BT_NAME_MAX_LEN - 1U] = 0U;
    s_bt_adv_data[0] = 0U;
}

void At_SyncFromModule(void)
{
    /*
     * 监控 JSON 的 btName/btAdvData 来自 MCU 缓存，不是自动从模块读的。
     * 已初始化过的板子开机后查询一次，避免监控里一直显示默认名。
     */
    if (DrvFlashStorage_IsBtConfigured() == 0)
    {
        return;
    }

    at_send_line("AT+NAME?");
    at_send_line("AT+RESE?");
}
