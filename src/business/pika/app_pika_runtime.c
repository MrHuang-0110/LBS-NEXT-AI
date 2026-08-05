#include "app_pika_runtime.h"
#include "monitor.h"
#include "PikaMain.h"
#include "PikaObj.h"
#include "PikaVM.h"
#include "motor.h"
#include "usbd_cdc_interface.h"
#include "drv_led.h"
#include "key.h"
extern volatile bool start_py;
#include <setjmp.h>
#include <string.h>

extern volatile PikaObj *__pikaMain;
extern unsigned char pikaModules_py_a[];

#define PIKA_MAGIC_PYO0   0x0FU
#define PIKA_MAGIC_PYO1   (uint8_t)'p'
#define PIKA_MAGIC_PYO2   (uint8_t)'y'
#define PIKA_MAGIC_PYO3   (uint8_t)'o'
#define PIKA_MAGIC_PYA3   (uint8_t)'a'

static jmp_buf s_run_jmp;
static volatile uint8_t s_stop_req;
static volatile AppPikaState_t s_state;
static uint8_t s_inited;
static const uint8_t *s_bytecode;
static uint32_t s_bytecode_len;

/* monitor 状态提供者：协议层经 Monitor_RegisterStateProvider 获取 Pika 运行状态，
 * �?protocol 层不依赖本模块的 AppPika_GetState */
static const char *app_monitor_state_provider(void)
{
    return (AppPika_GetState() == APP_PIKA_STATE_RUNNING) ? "run" : "stop";
}

static int pika_magic_is_pyo(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len < 4U)
    {
        return 0;
    }
    return (data[0] == PIKA_MAGIC_PYO0 && data[1] == PIKA_MAGIC_PYO1 &&
            data[2] == PIKA_MAGIC_PYO2 &&
            (data[3] == PIKA_MAGIC_PYO3 || data[3] == PIKA_MAGIC_PYA3)) ? 1 : 0;
}

void pika_hook_instruct(void)
{
    /* 非实时兜底：监控/命令轮询已移�?TIM6 事件回调（monitor_event/cmd_poll），
     * VM 阻塞期间仍保持实时；�?hook 仅在脚本执行间隙做轻量停止检查与
     * 挂起消费（关机序列含 Flash 写入+阻塞动画、cloase_all_motor �?60ms 忙等�?     * 均禁止在 ISR 内执行，�?VM 执行间隙兜底消费�?*/
    AppPika_CheckAbort();

    /* 长按关机挂起序列（禁止在 ISR 执行，由 VM 执行间隙消费�?*/
    extern volatile uint8_t g_shutdown_pending;
    if (g_shutdown_pending != 0U)
    {
        g_shutdown_pending = 0U;
        extern void app_shutdown_sequence(void);
        app_shutdown_sequence();   /* 不再返回 */
    }

    /* 短按停止挂起的电机复位（cloase_all_motor �?60ms 忙等，禁止在 ISR 内执行） */
    extern volatile uint8_t g_motor_stop_pending;
    if (g_motor_stop_pending != 0U)
    {
        g_motor_stop_pending = 0U;
        cloase_all_motor();
    }
}

int AppPika_IsStopRequested(void)
{
    return (s_stop_req != 0U) ? 1 : 0;
}

void AppPika_CheckAbort(void)
{
    if (s_stop_req != 0U)
    {
        longjmp(s_run_jmp, 1);
    }
}

static int app_pika_ensure_init(void)
{
    PikaObj *main_obj;
    if (s_inited != 0U && __pikaMain != NULL)
    {
        return 0;
    }
    if (__pikaMain != NULL)
    {
        obj_deinit((PikaObj *)__pikaMain);
        __pikaMain = NULL;
    }
    main_obj = newRootObj("pikaMain", New_PikaMain);
    if (main_obj == NULL)
    {
        return -1;
    }
    __pikaMain = main_obj;
    obj_linkLibrary(main_obj, pikaModules_py_a);
    s_inited = 1U;
  //  (void)usb_printf("\r\n[Pika] runtime ready\r\n");
    return 0;
}

int AppPika_LoadBytecode(const uint8_t *data, uint32_t len)
{
    if (data == NULL || len < 8U)
    {
        return -1;
    }
    if (pika_magic_is_pyo(data, len) == 0)
    {
        return -2;
    }
    if (app_pika_ensure_init() != 0)
    {
        return -3;
    }
    if (s_state == APP_PIKA_STATE_RUNNING)
    {
        (void)AppPika_Stop();
    }
    s_bytecode = data;
    s_bytecode_len = len;
    s_state = APP_PIKA_STATE_READY;
    (void)usb_printf("\r\n[Pika] bytecode loaded, %lu bytes\r\n", (unsigned long)len);
    return 0;
}

uint8_t AppPika_HasBytecode(void)
{
    return (s_bytecode != NULL && s_bytecode_len > 0U) ? 1U : 0U;
}

AppPikaState_t AppPika_GetState(void)
{
    return s_state;
}

int AppPika_Start(void)
{
    extern void pid_line_follow_reset(void);
    extern void loader_remote_cfg(void);
    extern void set_event_enable(char *name);

    if (s_bytecode == NULL || s_bytecode_len == 0U)
    {
        (void)usb_printf("\r\n[Pika] no bytecode, send .py.o via YMODEM first\r\n");
        return -1;
    }
    if (s_state == APP_PIKA_STATE_RUNNING)
    {
        return 0;
    }
    if (app_pika_ensure_init() != 0)
    {
        return -2;
    }

    s_stop_req = 0U;
    s_state = APP_PIKA_STATE_RUNNING;

    Monitor_RegisterStateProvider(app_monitor_state_provider);
    set_event_enable("monitor_event");
    loader_remote_cfg();
    pid_line_follow_reset();

    if (setjmp(s_run_jmp) == 0)
    {
        (void)pikaVM_runByteCodeInconstant((PikaObj *)__pikaMain, (uint8_t *)s_bytecode);
    }

    if (__pikaMain != NULL)
    {
        obj_deinit((PikaObj *)__pikaMain);
        __pikaMain = NULL;
    }
    s_inited = 0U;
    s_stop_req = 0U;
    cloase_all_motor();
    DrvLed_SetFlowEnable(1U);
    if (s_bytecode != NULL)
    {
        s_state = APP_PIKA_STATE_READY;
    }
    else
    {
        s_state = APP_PIKA_STATE_OFF;
    }
    start_py = false;
    (void)usb_printf("\r\n[Pika] script stopped\r\n");
    return 0;
}

int AppPika_Stop(void)
{
    if (s_state != APP_PIKA_STATE_RUNNING)
    {
        return 0;
    }
    s_stop_req = 1U;
    pks_vm_exit();
    s_stop_req = 0U;
    return 0;
}
