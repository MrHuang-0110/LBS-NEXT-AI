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
 *
 * C1 回归修复：完整行到达不再于 ISR 内执行（AT 交换/Ymodem 含 HAL_GetTick 超时，
 * TIM6 pri=2 > SysTick pri=15 时超时失效挂死），改为 ISR 入队 + g_cmd_pending 置位，
 * 由 Cmd_ProcessPendingLines 在线程上下文（主循环 / pika hook）消费执行。
 */
#include "cmd.h"
#include "at.h"
#include "drv_comm.h"
#include "usbd_cdc_interface.h"
#include <stdint.h>
#include <string.h>

#define USB_CMD_LINE_MAX        96U
#define BT_CMD_LINE_MAX         96U
#define CMD_PENDING_LINE_MAX    96U

typedef void (*CmdAction_t)(_AGREEMENT *frame);

/* 挂起完整文本行（ISR 入队，线程上下文消费）。
 * C1 回归修复：AT 交换 / Ymodem 依赖 HAL_GetTick，TIM6(pri=2) 内 SysTick(pri=15)
 * 被屏蔽时 HAL_GetTick 冻结，阻塞执行会导致系统挂死；故 ISR 仅入队置位，
 * 由 Cmd_ProcessPendingLines（主循环 / pika hook）在线程上下文执行。 */
volatile uint8_t g_cmd_pending = 0U;
static uint8_t s_pending_line[CMD_PENDING_LINE_MAX];
static DrvCommPort_t s_pending_port;

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
            /* 仅入队不执行：行命令（AT 交换/Ymodem）必须在线程上下文运行。
             * 上一行仍未处理时直接覆盖（last-line-wins）：AT/Ymodem 为串行、
             * 低频用户操作，可接受。 */
            (void)memcpy(s_pending_line, line, (size_t)(*idx) + 1U);
            s_pending_port = port;
            g_cmd_pending = 1U;
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

/* C1: 线程上下文消费挂起的完整行（主循环 / pika hook 调用）。
 * 循环内先拷贝到局部再清标志，处理期间新到达的行会被 while 重查发现，不丢失。 */
void Cmd_ProcessPendingLines(void)
{
    char line[CMD_PENDING_LINE_MAX];
    DrvCommPort_t port;

    while (g_cmd_pending != 0U)
    {
        (void)memcpy(line, s_pending_line, sizeof(s_pending_line));
        port = s_pending_port;
        g_cmd_pending = 0U;
        cmd_handle_line(line, port);
    }
}

/* Task15: USB 帧桥 —— middleware 的 usb_event_receive_callback 经注册点回调到本层，
 * protocol 不再被 middleware 直接调用（tx 发送函数由 middleware 自身持有，本层忽略） */
static void s_usb_frame_bridge(_AGREEMENT *frame, void (*tx)(void*, uint16_t))
{
    (void)tx;
    Cmd_ProcessFrame(frame);
}

void Cmd_Init(void)
{
    DrvUsbRing_Init();
    DrvBtRing_Init();
    s_ymodem_active = 0U;
    g_cmd_pending = 0U;
    s_usb_idx = 0U;
    s_bt_idx = 0U;
    Usb_RegisterFrameHandler(s_usb_frame_bridge);
}
