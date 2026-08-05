/**
 ****************************************************************************************************
 * @file        btim.h
 * @author      ??????????(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-20
 * @brief       ????????? ????????
 * @license     Copyright (c) 2020-2032, ??????????????????????
 ****************************************************************************************************
 * @attention
 *
 * ?????:??????? STM32F103??????
 * ???????:www.yuanzige.com
 * ???????:www.openedv.com
 * ??????:www.alientek.com
 * ??????:openedv.taobao.com
 *
 * ??????
 * V1.0 20211216
 * ????��???
 *
 ****************************************************************************************************
 */

#ifndef __BTIM_H
#define __BTIM_H

#include "sys.h"

#define PWM_MAX (100)

/******************************************************************************************/
/* ????????? ???? */

#define BTIM_TIMX_INT                       TIM6
#define BTIM_TIMX_INT_IRQn                  TIM6_IRQn
#define BTIM_TIMX_INT_IRQHandler            TIM6_IRQHandler
#define BTIM_TIMX_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM6_CLK_ENABLE(); }while(0)  


#define GTIM_TIM2_INT                       TIM2
#define GTIM_TIM2_INT_IRQn                  TIM2_IRQn
#define GTIM_TIM2_INT_IRQHandler            TIM2_IRQHandler
#define GTIM_TIM2_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)  

#define GTIM_TIM3_INT                       TIM3
#define GTIM_TIM3_INT_IRQn                  TIM3_IRQn
#define GTIM_TIM3_INT_IRQHandler            TIM3_IRQHandler
#define GTIM_TIM3_INT_CLK_ENABLE()          do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0) 

#define GTIM_M1A_PWM_CHY_GPIO_PORT         GPIOA
#define GTIM_M1A_PWM_CHY_GPIO_PIN          GPIO_PIN_15
#define GTIM_M1A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)  

#define GTIM_M1A_PWM                       TIM2 
#define GTIM_M1A_PWM_CHY                   TIM_CHANNEL_1                                
#define GTIM_M1A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR1                          
#define GTIM_M1A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)  

#define GTIM_M1B_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M1B_PWM_CHY_GPIO_PIN          GPIO_PIN_3
#define GTIM_M1B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  

#define GTIM_M1B_PWM                       TIM2 
#define GTIM_M1B_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_M1B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                          
#define GTIM_M1B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)   


#define GTIM_M2A_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M2A_PWM_CHY_GPIO_PIN          GPIO_PIN_6
#define GTIM_M2A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M2A_PWM                       TIM3 
#define GTIM_M2A_PWM_CHY                   TIM_CHANNEL_1                                
#define GTIM_M2A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR1                         
#define GTIM_M2A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  

#define GTIM_M2B_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M2B_PWM_CHY_GPIO_PIN          GPIO_PIN_7
#define GTIM_M2B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M2B_PWM                       TIM3 
#define GTIM_M2B_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_M2B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                         
#define GTIM_M2B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  


#define GTIM_M3A_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M3A_PWM_CHY_GPIO_PIN          GPIO_PIN_8
#define GTIM_M3A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)  

#define GTIM_M3A_PWM                       TIM3 
#define GTIM_M3A_PWM_CHY                   TIM_CHANNEL_3                                
#define GTIM_M3A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR3                          
#define GTIM_M3A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  

#define GTIM_M3B_PWM_CHY_GPIO_PORT         GPIOC
#define GTIM_M3B_PWM_CHY_GPIO_PIN          GPIO_PIN_9
#define GTIM_M3B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   

#define GTIM_M3B_PWM                       TIM3
#define GTIM_M3B_PWM_CHY                   TIM_CHANNEL_4                               
#define GTIM_M3B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR4                         
#define GTIM_M3B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  


#define GTIM_M4A_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M4A_PWM_CHY_GPIO_PIN          GPIO_PIN_8
#define GTIM_M4A_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  

#define GTIM_M4A_PWM                       TIM4 
#define GTIM_M4A_PWM_CHY                   TIM_CHANNEL_3                                
#define GTIM_M4A_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR3                          
#define GTIM_M4A_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  

#define GTIM_M4B_PWM_CHY_GPIO_PORT         GPIOB
#define GTIM_M4B_PWM_CHY_GPIO_PIN          GPIO_PIN_9
#define GTIM_M4B_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   

#define GTIM_M4B_PWM                       TIM4
#define GTIM_M4B_PWM_CHY                   TIM_CHANNEL_4                               
#define GTIM_M4B_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR4                         
#define GTIM_M4B_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  


#define GTIM_BUZZ_PWM_CHY_GPIO_PORT         GPIOA
#define GTIM_BUZZ_PWM_CHY_GPIO_PIN          GPIO_PIN_1
#define GTIM_BUZZ_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   

#define GTIM_BUZZ_PWM                       TIM5
#define GTIM_BUZZ_PWM_CHY                   TIM_CHANNEL_2                               
#define GTIM_BUZZ_PWM_CHY_CCRX              GTIM_TIMX_PWM->CCR2                        
#define GTIM_BUZZ_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM5_CLK_ENABLE(); }while(0)  

typedef struct {
    uint16_t year;   // ??????2025
    uint8_t  month;  // ?��? 1-12
    uint8_t  day;    // ???? 1-31
    uint8_t  hour;   // ��? 0-23
    uint8_t  minute; // ???? 0-59
    uint8_t  second; // ?? 0-59
} DateTime_t;

uint32_t getTim6Tick(void);
void btim_timx_int_init(uint16_t arr, uint16_t psc);
void btim_timx_int_start(void);    /* ????????? ????��????????? */
void pwm_init(void);

void pwm_set_output(uint8_t id,int pwm);
void pwm_stop_breaking(uint8_t id);
void pwm_stop_slide(uint8_t id);


void resetUserCPUTick(void);
uint32_t getUserCPUTick(void);


void Time_Init(void);
// ?????????
void Time_Set(DateTime_t *dt);
// ???????????
void Time_Get(DateTime_t *dt);
// ??????????????��????????
void Time_AddSeconds(int32_t seconds);
// ????????��??FLASH?????�_?��
void Time_SaveToFlash(void);
// ??FLASH????????????????
void Time_LoadFromFlash(void);


void SavePowerOnStartTimer(void);
void SavePowerDownTimer(void);

void GetPowerOnStartTimer(DateTime_t *dt);
void GetPoweDownTimer(DateTime_t *dt);

#endif

















