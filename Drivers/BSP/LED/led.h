
#ifndef _LED_H
#define _LED_H
#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* ???? ???? */
 
#if WIALL_HARDWARE_ENABLE
/* ????????PB0 ????PB1 ???????????? */
/* PB0=LED0 ��ƣ�PB1=LED1 �̵ƣ��͵�ƽ���� */
#define LED0_GPIO_PORT                  GPIOB
#define LED0_GPIO_PIN                   GPIO_PIN_0
#define LED0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define LED1_GPIO_PORT                  GPIOB
#define LED1_GPIO_PIN                   GPIO_PIN_1
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define LED2_GPIO_PORT                  GPIOC
#define LED2_GPIO_PIN                   GPIO_PIN_15
#define LED2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PD???????? */ 
#else

#define LED0_GPIO_PORT                  GPIOA
#define LED0_GPIO_PIN                   GPIO_PIN_8
#define LED0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)             /* PA???????? */

#define LED1_GPIO_PORT                  GPIOA
#define LED1_GPIO_PIN                   GPIO_PIN_2
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)             /* PD???????? */

#endif

/******************************************************************************************/
/* LED?????? */
#define LED0(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED0??? */

#define LED1(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED1??? */
#define LED2(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)      /* LED1??? */

/* LED??????? */
#define LED0_TOGGLE()   do{ HAL_GPIO_TogglePin(LED0_GPIO_PORT, LED0_GPIO_PIN); }while(0)        /* ???LED0 */
#define LED1_TOGGLE()   do{ HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN); }while(0)        /* ???LED1 */

#if WIALL_HARDWARE_ENABLE
#define LED2_TOGGLE()   do{ HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN); }while(0)        /* ???LED1 */
#endif

/******************************************************************************************/

/* ????????*/
void led_init(void);                                                                            /* ????? */

 
void led3_event_callback(void *arg);
#endif
