#include "main.h"
#include "motor.h"
#include "blue.h"
#include "ultrasion.h"
#include "touch.h"
#include "color.h"
#include "monitor.h"
#include "bat_manager.h"
#include "drv_led.h"
#include "app_cmd.h"
#include "cmd.h"
#include "app_pika_runtime.h"
#include "app_pika_script_flash.h"
#include "drv_comm.h"
#include "key.h"
#include "beep.h"
#include "drv_flash_storage.h"
#include "drv_bt_config.h"
#include "adc.h"
#include "drv_ir_reflect.h"
#include "PikaVM.h"

volatile bool is_iwdg = false;
volatile uint32_t spark_version = 100U;
volatile float bat = 0.0f;

/* 挂起关机标志：由按键回调（ISR 上下文）置位，由 hook/主循环消费执行关机序列
 * （禁止在 ISR 内执行 Flash 写入与阻塞动画） */
volatile uint8_t g_shutdown_pending = 0U;

/* 挂起电机停止标志：短按停止脚本时 cloase_all_motor() 含 60ms 忙等（delay_ms(30)x2），
 * 禁止在 ISR 内执行，由 hook/主循环消费 */
volatile uint8_t g_motor_stop_pending = 0U;

static EVENT_MANAGER event_t[] = {
 {"led_flow_event", 1, led_flow_event_callback, NULL},
 {"iwdg_feedevent", 10, iwdg_feed, NULL},
 {"usb_connect", 10, usb_event_connect_callback, NULL},
 {"scan_adc", 100, sample_adc_data_callback, NULL},
 {"scan_ir_reflect", 50, drv_ir_reflect_sample_callback, NULL},
 {"usb_receive", 0, usb_event_receive_callback, (USB_MESSAGE_BOX *)&usb_message},
 {"beep", 1, beep_update, NULL},
 {"key_middle_event", 1, key_middle_callback, NULL},
 {"monitor_event", 1, monitor_call_back, NULL},
 {"battery_check", 600, battery_check_callback, NULL},
 {"cmd_poll", 1, Cmd_PollCallback, NULL}
};

static uint8_t check_swd_config(void)
{
    uint32_t mapr_value = AFIO->MAPR;
    return (uint8_t)((mapr_value >> 24) & 0x7U);
}

static void app_pwr_hold_enable(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = APP_PWR_CTRL_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(APP_PWR_CTRL_GPIO_PORT, &gpio);
    HAL_GPIO_WritePin(APP_PWR_CTRL_GPIO_PORT, APP_PWR_CTRL_GPIO_PIN, GPIO_PIN_SET);
}

static void disable_jtag_enable_swd(void)
{
    /* 010：关闭 JTAG，保留 SWD，便于 Keil 仿真 */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR &= ~AFIO_MAPR_SWJ_CFG_Msk;
    AFIO->MAPR |= (0x1U << 24);
    __DSB();
    __ISB();
    /* 勿在调试阶段因读回不等 2 就拉低 PC5 死循环，否则表现为“后面所有 init 都卡住” */
    (void)check_swd_config();
}

void app_shutdown_sequence(void)
{
    beep_stop();
    (void)AppPikaScriptFlash_SaveFromRam();   /* 关机前保存当前脚本到 Flash */
    DrvLed_SetFlowEnable(0U);
    DrvLed_PlayShutdownAnimationBlocking();
    pika_vmSignal_setCtrlClear();
    beep_play_shutdown_melody_blocking();
    HAL_GPIO_WritePin(APP_PWR_CTRL_GPIO_PORT, APP_PWR_CTRL_GPIO_PIN, GPIO_PIN_RESET);
    while (1)
    {
        HAL_Delay(1000);
    }
}

/* ---- 按键动作回调（注册给 key.c 状态机；在 TIM6 事件回调 / ISR 上下文执行，
 *       必须非阻塞：不写 Flash、不阻塞延时） ---- */
static uint8_t s_hold_lit_prev;
static uint8_t s_hold_active;   /* 按住中：按下开始时暂停流水灯，短按恢复时复位 */

static void app_key_short_press(void)
{
    /* ISR 安全：不调用 AppPika_OnKeyToggle（其 RUNNING 分支内含 cloase_all_motor 的
     * 60ms 忙等）。RUNNING→Stop 仅置标志，电机停止挂起由 hook/主循环消费 */
    if (AppPika_GetState() == APP_PIKA_STATE_RUNNING)
    {
        (void)AppPika_Stop();       /* ISR 安全：仅置停止标志 + pks_vm_exit */
        g_motor_stop_pending = 1U;  /* cloase_all_motor 含 60ms 忙等，移到主循环/hook 消费 */
    }
    else if (AppPika_HasBytecode() != 0U)
    {
        start_py = true;            /* 与 0xB6 命令在 ISR 写 start_py 同模式，安全 */
    }
    s_hold_active = 0U;
    DrvLed_SetFlowEnable(1U);
}

static void app_key_release(void)
{
    /* 非短按释放（held >= 800ms 且未触发短按）：恢复流水灯，与原版释放分支一致 */
    s_hold_active = 0U;
    DrvLed_SetFlowEnable(1U);
}

static void app_key_long_press(void)
{
    g_shutdown_pending = 1U;   /* 由 hook/主循环执行关机序列（禁止在 ISR 内写 Flash/阻塞动画） */
}

static void app_key_hold_progress(uint8_t lit)
{
    DrvLed_ShowHoldProgress(lit);
    if (lit == 0U)
    {
        s_hold_lit_prev = 0U;   /* 新一次按下：复位档位记录，保证每档蜂鸣节奏与原版一致 */
        if (s_hold_active == 0U)
        {
            s_hold_active = 1U;
            DrvLed_SetFlowEnable(0U);   /* 按住开始：暂停流水灯，与原版按下瞬间行为一致 */
        }
        return;
    }
    if (lit > s_hold_lit_prev)
    {
        /* 非阻塞蜂鸣（ISR 安全）。原阻塞式 beep_play_melody 内含 HAL_Delay，
         * 在 TIM6 ISR 中调用会阻塞/死锁，故改用非阻塞 beep_play */
        beep_play(BEEP_KEY_PRESS);
        s_hold_lit_prev = lit;
    }
}

static void systemInit(void)
{
    (void)HAL_RCC_DeInit();
    HAL_Init();
    sys_stm32_clock_init(RCC_PLL_MUL15);
    delay_init(120);
    app_pwr_hold_enable();
    disable_jtag_enable_swd();
    my_mem_init(SRAMIN);
    btim_timx_int_init(120 - 1, 1000 - 1);
    usb_cdc_init();
    uart_port_init();
    adc_nch_dma_init();
    drv_ir_reflect_init();
    battery_manager_init();
    led_init();
    DrvLed_Init();
    key_init();
    iic_init();
    pwm_init();
    /* Task12: 设备注册表——业务模块在启动时注册 create/set_param/destroy，
     * device_pool 通过注册表分发，不再 include 业务头 */
    DevicePool_Register(DEVICE_MOTOR_ID, create_motor, NULL, destroy_motor);
    DevicePool_Register(DEVICE_ULTRASION_ID, create_ultrasion, refsh_ultrasion, destroy_ultrasion);
    DevicePool_Register(DEVICE_COLOR_ID, create_color_cfg, refsh_color, destroy_color);
    DevicePool_Register(DEVICE_TOUCH_ID, create_touch, refsh_touch, destroy_touch);
    DevicePool_Register(DEVICE_BLUE_ID, create_blue, refsh_blue, destroy_blue);
    init_identify_dev();
    cloase_all_motor();
    beep_init();
    AppCmd_Init();
    Time_Init();
    Key_Config_Params(10, 3000, 500);
   // iwdg_init(IWDG_PRESCALER_64, 625);
    for (uint32_t i = 0; i < sizeof(event_t) / sizeof(event_t[0]); i++)
    {
        create_event_manger(&event_t[i]);
    }
    uart_dma_idle_start();
    uart_blue_idle_start();
    DrvLed_SetFlowEnable(1U);
    set_event_enable("monitor_event");
    btim_timx_int_start();
    adc_nch_dma_start();
    /* Boot 跳转前 __disable_irq()，此处必须开中断，否则 SysTick/TIM6 不运行、HAL_GetTick 冻结 */
    __enable_irq();
    delay_ms(DRV_BT_AT_GAP_MS);
    blue_init();
    (void)AppPikaScriptFlash_LoadToRam();   /* 若 Flash 有有效脚本, 装载到 RAM (不运行) */
    AppCmd_SyncBtFromModule();

    /* Task3: 注册按键回调（状态机驱动于 TIM6 事件回调）并设置开机忽略窗口 */
    Key_RegisterShortPressCb(app_key_short_press);
    Key_RegisterLongPressCb(app_key_long_press);
    Key_RegisterHoldProgressCb(app_key_hold_progress);
    Key_RegisterReleaseCb(app_key_release);
    Key_EnableAfterBoot();
}

int main(void)
{
    sys_nvic_set_vector_table(FLASH_BASE, 0x8000U);
    systemInit();
    while (1)
    {
        /* 消费挂起关机标志（按键回调在 ISR 置位；关机序列含 Flash 写入+阻塞动画，仅在主循环执行） */
        if (g_shutdown_pending != 0U)
        {
            g_shutdown_pending = 0U;
            app_shutdown_sequence();
        }
        /* 消费挂起电机停止标志（cloase_all_motor 含 60ms 忙等，仅在主循环执行） */
        if (g_motor_stop_pending != 0U)
        {
            g_motor_stop_pending = 0U;
            cloase_all_motor();
        }
        if (start_py)
        {
            (void)AppPika_Start();
        }
    }
}
