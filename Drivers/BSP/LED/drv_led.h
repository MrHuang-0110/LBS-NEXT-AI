#ifndef DRV_LED_H
#define DRV_LED_H

#include <stdint.h>

void DrvLed_Init(void);
void DrvLed_FlowTaskPoll(void);
void DrvLed_SetFlowEnable(uint8_t enable);
void DrvLed_SetFlowFast(uint8_t fast);
void DrvLed_ShowHoldProgress(uint8_t lit_count);
void DrvLed_PlayShutdownAnimationBlocking(void);
void DrvLed_PollBtLink(uint8_t connected);
void led_flow_event_callback(void *arg);

#endif
