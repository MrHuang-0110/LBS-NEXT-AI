#include "drv_led.h"
#include "drv_bt_config.h"
#include "key.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f1xx_hal.h"

#define LED_ON                      0U
#define LED_OFF                     1U
#define LED_FLOW_PERIOD_NORMAL_MS   150U
#define LED_FLOW_PERIOD_FAST_MS     40U

#if WIALL_HARDWARE_ENABLE
#define LED_FLOW_COUNT              9U
#define LED_BT_GPIO_PORT            GPIOC
#define LED_BT_GPIO_PIN             GPIO_PIN_15
#else
#define LED_FLOW_COUNT              2U
#define LED_BT_GPIO_PORT            GPIOA
#define LED_BT_GPIO_PIN             GPIO_PIN_2
#endif

/* 流水灯引脚映射：支持跨 GPIO 端口 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} LedFlowPin_t;

#if WIALL_HARDWARE_ENABLE
static const LedFlowPin_t s_flow_pin[LED_FLOW_COUNT] = {
    {GPIOC, GPIO_PIN_0},   /* [1] */
    {GPIOC, GPIO_PIN_1},   /* [2] */
    {GPIOC, GPIO_PIN_2},   /* [3] */
    {GPIOC, GPIO_PIN_3},   /* [4] */
    {GPIOC, GPIO_PIN_13},  /* [5] */
    {GPIOB, GPIO_PIN_7},   /* [6] */
    {GPIOB, GPIO_PIN_6},   /* [7] */
    {GPIOB, GPIO_PIN_5},   /* [8] */
    {GPIOB, GPIO_PIN_2},   /* [9] */
};
#else
static const uint16_t s_flow_pin[LED_FLOW_COUNT] =
{
#if WIALL_HARDWARE_ENABLE
    /* 与 APP 一致：PC0~PC3 四灯流水；电池灯在 PB0/PB1，勿占用 PC13 */
    GPIO_PIN_0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
#else
    GPIO_PIN_8,
    GPIO_PIN_2,
#endif
};
#endif

static uint8_t s_flow_enable;
static uint8_t s_flow_idx;
static int8_t s_flow_dir;
static uint32_t s_last_ms;
static uint16_t s_flow_period_ms = LED_FLOW_PERIOD_NORMAL_MS;
static uint32_t s_bt_blink_last_ms;
static uint8_t s_bt_blink_on;

static void led_pin_set(uint8_t idx, uint8_t on)
{
    if (idx >= LED_FLOW_COUNT)
    {
        return;
    }
#if WIALL_HARDWARE_ENABLE
    HAL_GPIO_WritePin(s_flow_pin[idx].port, s_flow_pin[idx].pin,
                      (on == LED_ON) ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(GPIOA, s_flow_pin[idx],
                      (on == LED_ON) ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

static void flow_all_off(void)
{
    uint8_t i;
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, LED_OFF);
    }
}

static void flow_show(uint8_t idx)
{
    uint8_t i;
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, (i == idx) ? LED_ON : LED_OFF);
    }

}

static void bt_led_pin_set(uint8_t on)
{
    HAL_GPIO_WritePin(LED_BT_GPIO_PORT, LED_BT_GPIO_PIN,
                      (on == LED_ON) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void DrvLed_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;

#if WIALL_HARDWARE_ENABLE
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    /* 逐个初始化流水灯引脚（跨 GPIOC 和 GPIOB） */
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        gpio.Pin = s_flow_pin[i].pin;
        HAL_GPIO_Init(s_flow_pin[i].port, &gpio);
        HAL_GPIO_WritePin(s_flow_pin[i].port, s_flow_pin[i].pin, GPIO_PIN_SET);
    }

    gpio.Pin = LED_BT_GPIO_PIN;
    HAL_GPIO_Init(LED_BT_GPIO_PORT, &gpio);
    bt_led_pin_set(LED_OFF);
#else
    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        gpio.Pin = s_flow_pin[i];
        HAL_GPIO_Init(GPIOA, &gpio);
        HAL_GPIO_WritePin(GPIOA, s_flow_pin[i], GPIO_PIN_SET);
    }
    gpio.Pin = LED_BT_GPIO_PIN;
    HAL_GPIO_Init(LED_BT_GPIO_PORT, &gpio);
    bt_led_pin_set(LED_OFF);
#endif

    s_flow_enable = 0U;
    s_flow_idx = 0U;
    s_flow_dir = 1;
    s_last_ms = 0U;
    s_bt_blink_last_ms = 0U;
    s_bt_blink_on = 0U;
}

void DrvLed_SetFlowEnable(uint8_t enable)
{
    s_flow_enable = (enable != 0U) ? 1U : 0U;
    if (s_flow_enable != 0U)
    {
        flow_show(s_flow_idx);
    }
    else
    {
        flow_all_off();
    }
}

void DrvLed_SetFlowFast(uint8_t fast)
{
    s_flow_period_ms = (fast != 0U) ? LED_FLOW_PERIOD_FAST_MS : LED_FLOW_PERIOD_NORMAL_MS;
}

void DrvLed_FlowTaskPoll(void)
{
    uint32_t now;

    if (s_flow_enable == 0U)
    {
        return;
    }

    now = HAL_GetTick();
    if ((now - s_last_ms) < s_flow_period_ms)
    {
        return;
    }
    s_last_ms = now;

    if (s_flow_dir > 0)
    {
        if (s_flow_idx >= (LED_FLOW_COUNT - 1U))
        {
            s_flow_dir = -1;
        }
        else
        {
            s_flow_idx++;
        }
    }
    else
    {
        if (s_flow_idx == 0U)
        {
            s_flow_dir = 1;
        }
        else
        {
            s_flow_idx--;
        }
    }
    flow_show(s_flow_idx);
}

void DrvLed_ShowHoldProgress(uint8_t lit_count)
{
    uint8_t i;

    if (lit_count > LED_FLOW_COUNT)
    {
        lit_count = LED_FLOW_COUNT;
    }
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, (i < lit_count) ? LED_ON : LED_OFF);
    }
}

void DrvLed_PlayShutdownAnimationBlocking(void)
{
    uint8_t i;
    uint8_t step;

    for (step = 0U; step < 6U; step++)
    {
        for (i = 0U; i < LED_FLOW_COUNT; i++)
        {
            led_pin_set(i, ((step & 1U) == 0U) ? LED_ON : LED_OFF);
        }
        HAL_Delay(80);
    }

    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, LED_ON);
    }
    HAL_Delay(120);

    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, LED_OFF);
        HAL_Delay(100);
    }

    flow_all_off();
}

void DrvLed_SetPoint(uint8_t point)
{
    uint8_t i;

    /* 手动控制模式：关闭流水动画 */
    s_flow_enable = 0U;

    if (point == 0U)
    {
        flow_all_off();
        return;
    }

    if (point > LED_FLOW_COUNT)
    {
        return;
    }

    /* point 是 1-indexed，点亮对应 LED */
    for (i = 0U; i < LED_FLOW_COUNT; i++)
    {
        led_pin_set(i, (i == (point - 1U)) ? LED_ON : LED_OFF);
    }
}

void DrvLed_SetPointState(uint8_t point, uint8_t on)
{
    if (point >= LED_FLOW_COUNT)
    {
        return;
    }

    /* 手动控制模式：关闭流水动画 */
    s_flow_enable = 0U;

    led_pin_set(point, (on != 0U) ? LED_ON : LED_OFF);
}

void DrvLed_PollBtLink(uint8_t connected)
{
    uint32_t now;

    /* PC15 蓝牙灯与 PC0~PC3 流水灯独立，流水开启时也应闪烁/常亮 */
    if (connected != 0U)
    {
        bt_led_pin_set(LED_ON);
        return;
    }
    now = HAL_GetTick();
    if ((now - s_bt_blink_last_ms) < DRV_BT_LED_BLINK_MS)
    {
        return;
    }
    s_bt_blink_last_ms = now;
    s_bt_blink_on = (uint8_t)(s_bt_blink_on ^ 1U);
    bt_led_pin_set(s_bt_blink_on);
}

void led_flow_event_callback(void *arg)
{
    (void)arg;
    DrvLed_FlowTaskPoll();
#if WIALL_HARDWARE_ENABLE
    DrvLed_PollBtLink(BLUE_STA ? 1U : 0U);
#endif
}
