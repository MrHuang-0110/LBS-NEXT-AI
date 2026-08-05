#include "adc.h"
#include "bat_manager.h"

/* 仅 PA0 电池电压；轮询采样，避免连续 DMA 占满 CPU/在 TIM 事件里阻塞 */
static ADC_HandleTypeDef g_adc_handle = {0};

uint16_t g_adc_buf[SUM_ADC_CHANNL];
float    g_adc_voltage[SUM_ADC_CHANNL];

extern volatile float bat;

static uint16_t adc_read_battery_raw(void)
{
    if (HAL_ADC_Start(&g_adc_handle) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&g_adc_handle, 20U) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&g_adc_handle);
        return 0U;
    }
    uint16_t val = (uint16_t)HAL_ADC_GetValue(&g_adc_handle);
    (void)HAL_ADC_Stop(&g_adc_handle);
    return val;
}

void adc_nch_dma_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};
    ADC_ChannelConfTypeDef adc_ch_conf = {0};

    ADC_ADCX_CHY_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);

    gpio_init_struct.Pin = GPIO_PIN_0;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    g_adc_handle.Instance = ADC_ADCX;
    g_adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_adc_handle.Init.ScanConvMode = ADC_SCAN_DISABLE;
    g_adc_handle.Init.ContinuousConvMode = DISABLE;
    g_adc_handle.Init.NbrOfConversion = 1;
    g_adc_handle.Init.DiscontinuousConvMode = DISABLE;
    g_adc_handle.Init.NbrOfDiscConversion = 0;
    g_adc_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    HAL_ADC_Init(&g_adc_handle);
    HAL_ADCEx_Calibration_Start(&g_adc_handle);

    adc_ch_conf.Channel = ADC_CHANNEL_0;
    adc_ch_conf.Rank = ADC_REGULAR_RANK_1;
    adc_ch_conf.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    HAL_ADC_ConfigChannel(&g_adc_handle, &adc_ch_conf);
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

    g_adc_buf[0] = adc_read_battery_raw();
    g_adc_voltage[0] = (float)g_adc_buf[0] * (3.3f / 4096.0f);
    bat = g_adc_voltage[0];
    update_battery_voltage(g_adc_voltage[0]);
}
