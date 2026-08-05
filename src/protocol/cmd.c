/*
 * cmd.c — 命令通道层（协议帧分发 + 文本行命令轮询 + 命令执行注册表）
 *
 * 自 Users/app_cmd.c 拆出（Task 9）：
 *  - busDataparsing      -> Cmd_ProcessFrame（走动作注册表，未注册 index 忽略）
 *  - app_handle_cmd_line -> cmd_handle_line（经 Cmd_RegisterLineHandler 转发业务层，
 *                            避免 protocol 层依赖 business 的 ymodem/AT 实现）
 *  - app_feed_byte       -> cmd_feed_byte
 *  - AppCmd_PollUsb/Bt   -> Cmd_PollUsb/Cmd_PollBt（由 Cmd_PollCallback 事件调用）
 *  - AppCmd_Init 的 ring 初始化部分 -> Cmd_Init
 *  - s_ymodem_active     -> 迁入本层（Cmd_SetYmodemActive/Cmd_IsYmodemActive），
 *                            ymodem 执行期间暂停 USB/BT 命令轮询与 USB 数据入队
 */
#include "cmd.h"
#include "at.h"
#include "drv_comm.h"
#include <stdint.h>

#define USB_CMD_LINE_MAX        96U
#define BT_CMD_LINE_MAX         96U

typedef void (*CmdAction_t)(_AGREEMENT *frame);

static CmdAction_t s_actions[256];
static void (*s_line_handler)(const char *line, uint8_t port);
static volatile uint8_t s_ymodem_active;        /* ymodem 传输占用通道期间暂停命令轮询 */
static uint8_t s_usb_line[USB_CMD_LINE_MAX];
static uint8_t s_usb_idx;
static uint8_t s_bt_line[BT_CMD_LINE_MAX];
static uint8_t s_bt_idx;

void Cmd_RegisterAction(uint8_t index, void (*action)(_AGREEMENT *frame))
{
    /* index 为 uint8_t（0~255），s_actions 长度 256，恒有效 */
    s_actions[index] = action;
}

void Cmd_ProcessFrame(_AGREEMENT *frame)
{
    if (frame == NULL)
    {
        return;
    }
    if (s_actions[frame->index] != NULL)
    {
        s_actions[frame->index](frame);
    }
}

void Cmd_RegisterLineHandler(void (*handler)(const char *line, uint8_t port))
{
    s_line_handler = handler;
}

void Cmd_SetYmodemActive(uint8_t on)
{
    s_ymodem_active = (on != 0U) ? 1U : 0U;
}

uint8_t Cmd_IsYmodemActive(void)
{
    return s_ymodem_active;
}

/* 完整文本行到达：转交业务层注册的 line handler */
static void cmd_handle_line(const char *line, DrvCommPort_t port)
{
    if (line == NULL)
    {
        return;
    }
    if (s_line_handler != NULL)
    {
        s_line_handler(line, (uint8_t)port);
    }
}

static void cmd_feed_byte(uint8_t b, uint8_t *line, uint8_t *idx, uint8_t max_len, DrvCommPort_t port)
{
    if (b == '\r' || b == '\n')
    {
        if (*idx > 0U)
        {
            line[*idx] = 0U;
            cmd_handle_line((char *)line, port);
            *idx = 0U;
        }
        return;
    }
    if (*idx < (max_len - 1U))
    {
        line[(*idx)++] = b;
    }
}

void Cmd_PollUsb(void)
{
    uint8_t b;
    if (s_ymodem_active != 0U)
    {
        return;
    }
    while (DrvUsbRing_ReadByte(&b, 0U))
    {
        cmd_feed_byte(b, s_usb_line, &s_usb_idx, USB_CMD_LINE_MAX, DRV_COMM_USB);
    }
}

void Cmd_PollBt(void)
{
    uint8_t b;
    if ((s_ymodem_active != 0U) || (At_IsBusy() != 0U))
    {
        return;
    }
    while (DrvBtRing_ReadByte(&b, 0U))
    {
        cmd_feed_byte(b, s_bt_line, &s_bt_idx, BT_CMD_LINE_MAX, DRV_COMM_UART5);
    }
}

/* Task5: cmd_poll 事件回调（TIM6 ISR，阈值 1 tick）。VM 阻塞期间命令轮询保持实时。
 * 内部沿用 PollUsb/PollBt 的 s_ymodem_active / At_IsBusy 门控，行为不变。 */
void Cmd_PollCallback(void *arg)
{
    (void)arg;
    Cmd_PollUsb();
    Cmd_PollBt();
}

void Cmd_Init(void)
{
    DrvUsbRing_Init();
    DrvBtRing_Init();
    s_ymodem_active = 0U;
    s_usb_idx = 0U;
    s_bt_idx = 0U;
}
