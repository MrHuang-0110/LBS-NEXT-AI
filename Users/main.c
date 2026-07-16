#include "main.h"
#include "motor.h"
#include "monitor.h"
#include "bat_manager.h"
#include "drv_led.h"
#include "app_cmd.h"
#include "app_pika_runtime.h"
#include "app_pika_script_flash.h"
#include "drv_comm.h"
#include "key.h"
#include "beep.h"
#include "drv_flash_storage.h"
#include "drv_bt_config.h"
#include "adc.h"
#include "drv_ir_reflect.h"

volatile bool is_iwdg = false;
volatile uint32_t spark_version = 100U;
volatile float bat = 0.0f;

#define KEY_HOLD_SHUTDOWN_MS    2000U
#define KEY_BOOT_IGNORE_MS      800U
#define KEY_CLICK_TOGGLE_MAX_MS 800U
#define KEY_CLICK_FREQ_HZ       880U
#define KEY_CLICK_BEEP_MS       40U

static uint32_t s_key_ready_tick;
static uint8_t s_key_ready_inited;

static EVENT_MANAGER event_t[] = {
 {"led_flow_event", 1, led_flow_event_callback, NULL},
 {"iwdg_feedevent", 10, iwdg_feed, NULL},
 {"usb_connect", 10, usb_event_connect_callback, NULL},
 {"scan_adc", 100, sample_adc_data_callback, NULL},
 {"scan_ir_reflect", 50, drv_ir_reflect_sample_callback, NULL},
 {"usb_receive", 0, usb_event_receive_callback, (USB_MESSAGE_BOX *)&usb_message},
 {"beep", 1, beep_update, NULL},
 {"key_middle_event", 1, key_middle_callback, NULL},
 {"monitor_event", 1, monitor_call_back, NULL}
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

static void app_shutdown_sequence(void)
{
    beep_stop();
    (void)AppPikaScriptFlash_SaveFromRam();   /* 关机前保存当前脚本到 Flash */
    DrvLed_SetFlowEnable(0U);
    DrvLed_PlayShutdownAnimationBlocking();
    beep_play_shutdown_melody_blocking();
    HAL_GPIO_WritePin(APP_PWR_CTRL_GPIO_PORT, APP_PWR_CTRL_GPIO_PIN, GPIO_PIN_RESET);
    while (1)
    {
        HAL_Delay(1000);
    }
}

static void Main_Loop_Process(void)
{
    static uint32_t press_start_tick = 0U;
    static uint8_t flow_paused = 0U;
    static uint8_t shutdown_done = 0U;
    static uint8_t hold_lit_prev = 0U;

    if (shutdown_done != 0U)
    {
        return;
    }

    if (s_key_ready_inited == 0U)
    {
        return;
    }
    if ((HAL_GetTick() - s_key_ready_tick) < KEY_BOOT_IGNORE_MS)
    {
        return;
    }

    Key_Scan_Handler(5);

    if (Key_Is_Pressed())
    {
        if (press_start_tick == 0U)
        {
            press_start_tick = HAL_GetTick();
            hold_lit_prev = 0U;
            if (flow_paused == 0U)
            {
                DrvLed_SetFlowEnable(0U);
                flow_paused = 1U;
            }
        }
        else
        {
            uint32_t held_ms = HAL_GetTick() - press_start_tick;
            uint8_t lit = (uint8_t)((held_ms * 4U) / KEY_HOLD_SHUTDOWN_MS);
            if (lit > 4U)
            {
                lit = 4U;
            }
            DrvLed_ShowHoldProgress(lit);
            if (lit > hold_lit_prev)
            {
                beep_play_melody("880", KEY_CLICK_BEEP_MS);
                hold_lit_prev = lit;
            }
            if (held_ms >= KEY_HOLD_SHUTDOWN_MS)
            {
                shutdown_done = 1U;
                app_shutdown_sequence();
            }
        }
    }
    else if (press_start_tick != 0U)
    {
        uint32_t press_duration_ms = HAL_GetTick() - press_start_tick;
        if ((press_duration_ms > 30U) && (press_duration_ms < KEY_CLICK_TOGGLE_MAX_MS))
        {
            /* 先恢复流水再启动脚本：start_py 标志由主循环在 Main_Loop_Process 返回后统一处理 */
            if (flow_paused != 0U)
            {
                DrvLed_SetFlowEnable(1U);
                flow_paused = 0U;
            }
            AppPika_OnKeyToggle();
        }
        else if (flow_paused != 0U)
        {
            DrvLed_SetFlowEnable(1U);
            flow_paused = 0U;
        }
        press_start_tick = 0U;
        hold_lit_prev = 0U;
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
    init_identify_dev();
    cloase_all_motor();
    beep_init();
    AppCmd_Init();
    Time_Init();
    Key_Config_Params(10, 1500, 500);
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
    s_key_ready_tick = HAL_GetTick();
    s_key_ready_inited = 1U;
}

int main(void)
{
    sys_nvic_set_vector_table(FLASH_BASE, 0x8000U);
    systemInit();
    while (1)
    {
        Main_Loop_Process();
        if (start_py)
        {
            (void)AppPika_Start();
        }
        Monitor_Poll();
        AppCmd_PollUsb();
        AppCmd_PollBt();
        check_battery_with_debounce();
        //HAL_Delay(5);
    }
}
