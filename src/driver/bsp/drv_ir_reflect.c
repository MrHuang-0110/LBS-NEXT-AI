#include "drv_ir_reflect.h"
#include "adc_sample.h"
#include "sys.h"

#define IR_EMIT_GPIO_PORT           GPIOC
#define IR_EMIT_GPIO_PIN            GPIO_PIN_14

#define IR_ADC_GPIO_PORT            GPIOA
#define IR_ADC_GPIO_PIN             GPIO_PIN_4

#define IR_ADC_INSTANCE             ADC2
#define IR_ADC_CHANNEL              ADC_CHANNEL_4

#define IR_SAMPLE_AVG_CNT           4U

static ADC_HandleTypeDef s_ir_adc_handle = {0};
static uint16_t s_ir_raw = 0U;

void drv_ir_reflect_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    AdcSample_ConfigClock();

    gpio_init.Pin = IR_EMIT_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IR_EMIT_GPIO_PORT, &gpio_init);
    HAL_GPIO_WritePin(IR_EMIT_GPIO_PORT, IR_EMIT_GPIO_PIN, GPIO_PIN_RESET);

    gpio_init.Pin = IR_ADC_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(IR_ADC_GPIO_PORT, &gpio_init);

    AdcSample_Init(&s_ir_adc_handle, IR_ADC_INSTANCE);
    AdcSample_ConfigChannel(&s_ir_adc_handle, IR_ADC_CHANNEL);

    drv_ir_reflect_sample();
}

void drv_ir_reflect_sample_callback(void *arg)
{
    (void)arg;
    drv_ir_reflect_sample();
}

void drv_ir_reflect_sample(void)
{
    uint32_t sum = 0U;

    for (uint32_t i = 0U; i < IR_SAMPLE_AVG_CNT; i++)
    {
        sum += (uint32_t)AdcSample_GetRaw(&s_ir_adc_handle);
    }
    s_ir_raw = (uint16_t)(sum / IR_SAMPLE_AVG_CNT);
}

uint16_t drv_ir_reflect_get_raw(void)
{
    return s_ir_raw;
}
