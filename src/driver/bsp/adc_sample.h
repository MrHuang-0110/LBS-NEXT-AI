#ifndef __ADC_SAMPLE_H
#define __ADC_SAMPLE_H

#include "stm32f1xx.h"

/* ADC 公共采样工具：消除 adc.c（电池电压）与 drv_ir_reflect.c（红外反射）
 * 中重复的"ADC 初始化骨架 + 单通道轮询采样"实现。
 * 各调用方自有的配置（ADC 外设时钟使能、GPIO 引脚、通道号、采样后处理）
 * 保留在各自模块；本工具只提供两处完全相同的部分。
 *
 * 说明：两个调用方当前均为轮询模式（非 DMA），故只提供单次轮询采样接口，
 * 未提供 StartDma。*/

/* 完成一次单通道轮询采样（Start -> PollForConversion(20ms) -> GetValue -> Stop）。
 * 失败返回 0。handle 为调用方各自持有的 ADC 句柄（ADC1 / ADC2）。 */
uint16_t AdcSample_GetRaw(ADC_HandleTypeDef *handle);

/* 配置 ADC 公共时钟：ADC 外设时钟选择 + ADCPCLK2 / 6 分频（两调用方一致）。
 * 调用方应先使能对应 ADC 外设时钟，再调用本函数。 */
void AdcSample_ConfigClock(void);

/* 初始化 ADC 句柄公共字段（右对齐 / 单通道 / 软件触发 / 1 次转换）并执行校准。 */
void AdcSample_Init(ADC_HandleTypeDef *handle, ADC_TypeDef *instance);

/* 配置单通道（Rank 1，采样时间 239.5 周期，两调用方一致）。 */
void AdcSample_ConfigChannel(ADC_HandleTypeDef *handle, uint32_t channel);

#endif /* __ADC_SAMPLE_H */
