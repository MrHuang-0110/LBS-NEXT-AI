#include "adc.h"
#include "adc_sample.h"
#include "bat_manager.h"

/* 仅 PA0 电池电压；轮询采样，避免连续 DMA 占满 CPU/在 TIM 事件里阻塞 */
static ADC_HandleTypeDef g_adc_handle = {0};

uint16_t g_adc_buf[SUM_ADC_CHANNL];
float    g_adc_voltage[SUM_ADC_CHANNL];

extern volatile float bat;

void adc_nch_dma_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    ADC_ADCX_CHY_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    AdcSample_ConfigClock();

    gpio_init_struct.Pin = GPIO_PIN_0;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    AdcSample_Init(&g_adc_handle, ADC_ADCX);
    AdcSample_ConfigChannel(&g_adc_handle, ADC_CHANNEL_0);
}

void adc_nch_dma_start(void)
{
    /* 轮询模式，无需启动 DMA */
}

float getBatValute(void)
{
    return g_adc_voltage[0];
}

void sample_adc_data_callback(void *arg)
{
    (void)arg;

    g_adc_buf[0] = AdcSample_GetRaw(&g_adc_handle);
    g_adc_voltage[0] = (float)g_adc_buf[0] * (3.3f / 4096.0f);
    bat = g_adc_voltage[0];
    update_battery_voltage(g_adc_voltage[0]);
}
