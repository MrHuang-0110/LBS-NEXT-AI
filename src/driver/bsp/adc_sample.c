#include "adc_sample.h"

uint16_t AdcSample_GetRaw(ADC_HandleTypeDef *handle)
{
    if (HAL_ADC_Start(handle) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(handle, 20U) != HAL_OK)
    {
        (void)HAL_ADC_Stop(handle);
        return 0U;
    }
    uint16_t val = (uint16_t)HAL_ADC_GetValue(handle);
    (void)HAL_ADC_Stop(handle);
    return val;
}

void AdcSample_ConfigClock(void)
{
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};

    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);
}

void AdcSample_Init(ADC_HandleTypeDef *handle, ADC_TypeDef *instance)
{
    handle->Instance = instance;
    handle->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    handle->Init.ScanConvMode = ADC_SCAN_DISABLE;
    handle->Init.ContinuousConvMode = DISABLE;
    handle->Init.NbrOfConversion = 1;
    handle->Init.DiscontinuousConvMode = DISABLE;
    handle->Init.NbrOfDiscConversion = 0;
    handle->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(handle);
    HAL_ADCEx_Calibration_Start(handle);
}

void AdcSample_ConfigChannel(ADC_HandleTypeDef *handle, uint32_t channel)
{
    ADC_ChannelConfTypeDef adc_ch_conf = {0};

    adc_ch_conf.Channel = channel;
    adc_ch_conf.Rank = ADC_REGULAR_RANK_1;
    adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(handle, &adc_ch_conf);
}
