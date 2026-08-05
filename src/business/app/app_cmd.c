/*
 * app_cmd.c — 命令执行层（business，Task 9 收敛）
 *
 * 保留：ymodem 流程执行（app_run_ymodem）、0xB6/0xB9/0xBE/0xBA 动作注册、
 * AppCmd_Init/AppCmd_SyncBtFromModule 调度入口、AppMonitor_* 上传暂停状态。
 * 已迁出：帧分发（-> Cmd_ProcessFrame）、文本行轮询（-> Cmd_PollUsb/Bt）、
 * AT 桥与蓝牙属性缓存（-> at.c）。
 */
#include "app_cmd.h"
#include "cmd.h"
#include "at.h"
#include "drv_comm.h"
#include "event_manager.h"
#include "app_pika_runtime.h"
#include "app_boot_param.h"
#include "beep.h"
#include "usbd_cdc_interface.h"
#include "delay.h"
#include "drv_mem.h"
#include "ymodem.h"
#include "key.h"
#include <string.h>
#include <stdio.h>

#define USB_CMD_FW_UPDATE       "ymodem update fmware"

static volatile uint8_t s_upload_paused;

void AppMonitor_SetUploadPaused(uint8_t paused)
{
    s_upload_paused = (paused != 0U) ? 1U : 0U;
}

uint8_t AppMonitor_IsUploadPaused(void)
{
    return s_upload_paused;
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
    Cmd_SetYmodemActive(1U);
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
    DrvBtRing_SetPassthrough(0U);
    Cmd_SetYmodemActive(0U);
}

/* 协议帧动作执行器（注册给 Cmd_RegisterAction） */
static void app_cmd_action_b6(_AGREEMENT *frame)
{
    (void)frame;
    if (AppPika_GetState() == APP_PIKA_STATE_RUNNING)
    {
        (void)AppPika_Stop();
    }
    else if (AppPika_HasBytecode() != 0U)
    {
        start_py = true;
        /* AppPika_Start() 由主循环统一调用，避免在 ISR 上下文中启动 VM */
    }
}

static void app_cmd_action_be(_AGREEMENT *frame)
{
    (void)frame;
    set_event_disable("monitor_event");
}

static void app_cmd_action_ba(_AGREEMENT *frame)
{
    (void)frame;
    set_event_enable("monitor_event");
}

/* 文本行命令处理（注册给 cmd 层 Cmd_RegisterLineHandler） */
static void app_cmd_line_handler(const char *line, uint8_t port)
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
        app_run_ymodem((DrvCommPort_t)port);
        return;
    }

    /* 以 "AT+" 开头的命令：仅 USB 下发；UART5 回显由 PollBt 读入，不可再次转发 */
    if ((line[0] == 'A' || line[0] == 'a') &&
        (line[1] == 'T' || line[1] == 't') &&
        line[2] == '+')
    {
        if (port != (uint8_t)DRV_COMM_USB)
        {
            return;
        }
        At_SendLine(line);
        return;
    }

    if (port == (uint8_t)DRV_COMM_USB)
    {
        (void)usb_printf("\r\nCMD:%s\r\n", line);
    }
}

void AppCmd_Init(void)
{
    Cmd_Init();
    Cmd_RegisterAction(0xB6, app_cmd_action_b6);
    Cmd_RegisterAction(0xB9, app_cmd_action_b6);
    Cmd_RegisterAction(0xBE, app_cmd_action_be);
    Cmd_RegisterAction(0xBA, app_cmd_action_ba);
    Cmd_RegisterLineHandler(app_cmd_line_handler);
    At_Init();
    s_upload_paused = 0U;
}

void AppCmd_SyncBtFromModule(void)
{
    At_SyncFromModule();
}
