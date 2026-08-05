#ifndef __ADC_H
#define __ADC_H

#include "sys.h"

#define ADC_ADCX                            ADC1
#define ADC_ADCX_CHY_CLK_ENABLE()           do{ __HAL_RCC_ADC1_CLK_ENABLE(); }while(0)

#define SUM_ADC_CHANNL   5

extern uint16_t g_adc_buf[SUM_ADC_CHANNL];
extern float    g_adc_voltage[SUM_ADC_CHANNL];
void adc_nch_dma_init(void);
void adc_nch_dma_start(void);
void sample_adc_data_callback(void *arg);
float getBatValute(void);
#endif
