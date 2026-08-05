#include "json-maker.h"
#include "monitor.h"
#include "at.h"
#include "device_pool.h"
#include "malloc.h"
#include <string.h>
#include <stdio.h>
#include "bat_manager.h"
#include "drv_ir_reflect.h"
#include "drv_comm.h"
#include "sys.h"
#if WIALL_HARDWARE_ENABLE
#include "key.h"
#endif

#define HUB_MONITOR_PORT_NUM        2U
#define APP_MONITOR_FW_VERSION      108U
#define MONITOR_USB_SEND_PERIOD_MS  1U
#define MONITOR_BT_SEND_PERIOD_MS   25U

static char json_buffer[2 * 1024];
static volatile uint8_t s_monitor_pending;
static uint32_t s_usb_monitor_last_ms;
static uint32_t s_bt_monitor_last_ms;
static volatile uint8_t s_upload_paused;
static MonitorStateProvider s_state_provider;

extern volatile uint32_t spark_version;
extern void usb_printf(char *fmt, ...);

#if WIALL_HARDWARE_ENABLE
static uint8_t monitor_bt_is_connected(void)
{
    return (BLUE_STA != 0) ? 1U : 0U;
}
#endif

void Monitor_SetUploadPaused(uint8_t paused)
{
    s_upload_paused = (paused != 0U) ? 1U : 0U;
}

uint8_t Monitor_IsUploadPaused(void)
{
    return s_upload_paused;
}

void Monitor_RegisterStateProvider(MonitorStateProvider cb)
{
    s_state_provider = cb;
}

static uint8_t monitor_build_json(void)
{
    size_t remLen = 2 * 1024;
    char *p = json_buffer;
    char temp_str[64];
    DeviceReading_t reading;

    memset(p, 0, remLen);
    HubBase_Scan_TimeOut();

    p = json_objOpen(p, NULL, &remLen);
    p = json_arrOpen(p, "deviceList", &remLen);
    for (uint8_t i = 0; i < HUB_MONITOR_PORT_NUM; i++)
    {
        p = json_objOpen(p, NULL, &remLen);
        p = json_int(p, "port", (int)i, &remLen);
        if (hub_port[i].sensors != NULL)
        {
            switch (hub_port[i].LinkeDeviceID)
            {
                case DEVICE_ULTRASION_ID:
                    memset(&reading, 0, sizeof(reading));
                    if (hub_port[i].sensors->read_values != NULL)
                    {
                        hub_port[i].sensors->read_values(hub_port[i].sensors, &reading);
                    }
                    p = json_objOpen(p, "ultrasion", &remLen);
                    snprintf(temp_str, sizeof(temp_str), "%d", reading.cm);
                    p = json_str(p, "cm", temp_str, &remLen);
                    p = json_objClose(p, &remLen);
                    break;
                case DEVICE_TOUCH_ID:
                    memset(&reading, 0, sizeof(reading));
                    if (hub_port[i].sensors->read_values != NULL)
                    {
                        hub_port[i].sensors->read_values(hub_port[i].sensors, &reading);
                    }
                    p = json_objOpen(p, "touch", &remLen);
                    p = json_int(p, "state", reading.touch_state, &remLen);
                    p = json_objClose(p, &remLen);
                    break;
                case DEVICE_COLOR_ID:
                    memset(&reading, 0, sizeof(reading));
                    if (hub_port[i].sensors->read_values != NULL)
                    {
                        hub_port[i].sensors->read_values(hub_port[i].sensors, &reading);
                    }
                    p = json_objOpen(p, "color", &remLen);
                    snprintf(temp_str, sizeof(temp_str), "%d", reading.lux);
                    p = json_str(p, "lux", temp_str, &remLen);
                    p = json_objClose(p, &remLen);
                    break;
                default:
                    break;
            }
        }
        p = json_objClose(p, &remLen);
    }
    p = json_arrClose(p, &remLen);

    p = json_objOpen(p, "adc", &remLen);
    snprintf(temp_str, sizeof(temp_str), "%d%%",
             calculate_battery_percentage(get_bat_filtered_volatge(), VOLTAGE_CRITICAL_LOW, VOLTAGE_HIGH_MAX));
    p = json_str(p, "bat", temp_str, &remLen);
    snprintf(temp_str, sizeof(temp_str), "%d", (int)drv_ir_reflect_get_raw());
    p = json_str(p, "ir", temp_str, &remLen);
    p = json_objClose(p, &remLen);

    p = json_int(p, "version", (int)spark_version, &remLen);
    snprintf(temp_str, sizeof(temp_str), "%d", my_mem_perused(SRAMIN));
    p = json_str(p, "heap", temp_str, &remLen);

    /* 蓝牙模块属性：始终输出，空时输出空字符串便于上位机识别 */
    {
        const char *bt_name = At_GetName();
        p = json_str(p, "btName", (bt_name != NULL) ? bt_name : "", &remLen);
    }
    {
        const char *bt_adv = At_GetAdvData();
        p = json_str(p, "btAdvData", (bt_adv != NULL) ? bt_adv : "", &remLen);
    }

    {
        const char *state = (s_state_provider != NULL) ? s_state_provider() : "stop";
        if (state != NULL && strcmp(state, "run") == 0)
        {
            p = json_str(p, "State", "run", &remLen);
        }
        else
        {
            p = json_str(p, "State", "stop", &remLen);
        }
    }

    p = json_objClose(p, &remLen);
    p = json_end(p, &remLen);
    return (p != NULL && remLen != 0) ? 1U : 0U;
}

void monitor_call_back(void *arg)
{
    uint32_t now;
    uint8_t need_usb = 0U;
    uint8_t need_bt = 0U;

    (void)arg;

    /* 保持原 TIM6 tick 置位语义：监控活跃期每次事件回调置位 pending，USB 发送成功后清零
     * （原 monitor_call_back 的置位逻辑随函数体整体移入，必须保留否则监控永不发送） */
    s_monitor_pending = 1U;

    /* TEMP-MEASURE: 上板实测后移除（Task4 Step3 风险门禁）
     * DWT 使能并清零 CYCCNT，函数末尾统计本回调(ISR)执行耗时；
     * 仅当 > 3000 cycles(≈41us@72MHz, tick 预算 1.67ms≈120000 cycles)
     * 时经 USB 打印，正常运行时无输出、不影响行为。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0U;

    if (Monitor_IsUploadPaused() != 0U)
    {
        return;
    }

    now = HAL_GetTick();
    if ((s_monitor_pending != 0U) &&
        ((now - s_usb_monitor_last_ms) >= MONITOR_USB_SEND_PERIOD_MS))
    {
        need_usb = 1U;
    }

#if WIALL_HARDWARE_ENABLE
    if ((monitor_bt_is_connected() != 0U) &&
        ((now - s_bt_monitor_last_ms) >= MONITOR_BT_SEND_PERIOD_MS))
    {
        need_bt = 1U;
    }
#endif

    if ((need_usb == 0U) && (need_bt == 0U))
    {
        return;
    }

    if (monitor_build_json() == 0U)
    {
        return;
    }

    if (need_usb != 0U)
    {
        usb_printf("%s\r\n", json_buffer);
        s_usb_monitor_last_ms = now;
        s_monitor_pending = 0U;
    }

#if WIALL_HARDWARE_ENABLE
    if (need_bt != 0U)
    {
        if (DrvComm_BtSendMonitor(json_buffer) != 0U)
        {
            s_bt_monitor_last_ms = now;
        }
    }
#endif

    /* TEMP-MEASURE: 上板实测后移除 —— 仅当 ISR 时长超预算才打印（N 应 < 3000） */
    if (DWT->CYCCNT > 3000U)
    {
        (void)usb_printf("[mon] isr %u cyc\r\n", (unsigned)(DWT->CYCCNT));
    }
}
