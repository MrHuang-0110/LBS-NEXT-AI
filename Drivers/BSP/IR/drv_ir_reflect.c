#include "drv_ir_reflect.h"
#include "./SYSTEM/sys/sys.h"

#define IR_EMIT_GPIO_PORT           GPIOC
#define IR_EMIT_GPIO_PIN            GPIO_PIN_14

#define IR_ADC_GPIO_PORT            GPIOA
#define IR_ADC_GPIO_PIN             GPIO_PIN_4

#define IR_ADC_INSTANCE             ADC2
#define IR_ADC_CHANNEL              ADC_CHANNEL_4

#define IR_SAMPLE_AVG_CNT           4U

static ADC_HandleTypeDef s_ir_adc_handle = {0};
static uint16_t s_ir_raw = 0U;

static uint16_t ir_read_adc_raw_once(void)
{
    if (HAL_ADC_Start(&s_ir_adc_handle) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&s_ir_adc_handle, 20U) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&s_ir_adc_handle);
        return 0U;
    }
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&s_ir_adc_handle);
    (void)HAL_ADC_Stop(&s_ir_adc_handle);
    return val;
}

void drv_ir_reflect_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};
    ADC_ChannelConfTypeDef adc_ch_conf = {0};

    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);

    gpio_init.Pin = IR_EMIT_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(IR_EMIT_GPIO_PORT, &gpio_init);
    HAL_GPIO_WritePin(IR_EMIT_GPIO_PORT, IR_EMIT_GPIO_PIN, GPIO_PIN_RESET);

    gpio_init.Pin = IR_ADC_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(IR_ADC_GPIO_PORT, &gpio_init);

    s_ir_adc_handle.Instance = IR_ADC_INSTANCE;
    s_ir_adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    s_ir_adc_handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
    s_ir_adc_handle.Init.ContinuousConvMode = DISABLE;
    s_ir_adc_handle.Init.NbrOfConversion = 1;
    s_ir_adc_handle.Init.DiscontinuousConvMode = DISABLE;
    s_ir_adc_handle.Init.NbrOfDiscConversion = 0;
    s_ir_adc_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&s_ir_adc_handle);
    HAL_ADCEx_Calibration_Start(&s_ir_adc_handle);

    adc_ch_conf.Channel = IR_ADC_CHANNEL;
    adc_ch_conf.Rank = ADC_REGULAR_RANK_1;
    adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&s_ir_adc_handle, &adc_ch_conf);

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
        sum += (uint32_t)ir_read_adc_raw_once();
    }
    s_ir_raw = (uint16_t)(sum / IR_SAMPLE_AVG_CNT);
}

uint16_t drv_ir_reflect_get_raw(void)
{
    return s_ir_raw;
}
