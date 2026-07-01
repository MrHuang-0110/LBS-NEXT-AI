 
#ifndef __USART_H
#define __USART_H

#include "stdio.h"
#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/*usart1*/
#define USART1_TX_GPIO_PORT                  GPIOA
#define USART1_TX_GPIO_PIN                   GPIO_PIN_9
#define USART1_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART1_RX_GPIO_PORT                  GPIOA
#define USART1_RX_GPIO_PIN                   GPIO_PIN_10
#define USART1_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART1_UX                            USART1
#define USART1_UX_IRQn                       USART1_IRQn
#define USART1_UX_IRQHandler                 USART1_IRQHandler
#define USART1_UX_CLK_ENABLE()               do{ __HAL_RCC_USART1_CLK_ENABLE(); }while(0)  /* USART1 ʱ��ʹ�� */

/******************************************************************************************/

/******************************************************************************************/
/*usart2*/
#define USART2_TX_GPIO_PORT                  GPIOA
#define USART2_TX_GPIO_PIN                   GPIO_PIN_2
#define USART2_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART2_RX_GPIO_PORT                  GPIOA
#define USART2_RX_GPIO_PIN                   GPIO_PIN_3
#define USART2_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART2_UX                            USART2
#define USART2_UX_IRQn                       USART2_IRQn
#define USART2_UX_IRQHandler                 USART2_IRQHandler
#define USART2_UX_CLK_ENABLE()               do{ __HAL_RCC_USART2_CLK_ENABLE(); }while(0)  /* USART1 ʱ��ʹ�� */

/******************************************************************************************/

/******************************************************************************************/
/*usart3*/
#define USART3_TX_GPIO_PORT                  GPIOB
#define USART3_TX_GPIO_PIN                   GPIO_PIN_10
#define USART3_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART3_RX_GPIO_PORT                  GPIOB
#define USART3_RX_GPIO_PIN                   GPIO_PIN_11
#define USART3_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART3_UX                            USART3
#define USART3_UX_IRQn                       USART3_IRQn
#define USART3_UX_IRQHandler                 USART3_IRQHandler
#define USART3_UX_CLK_ENABLE()               do{ __HAL_RCC_USART3_CLK_ENABLE(); }while(0)  /* USART1 ʱ��ʹ�� */

/******************************************************************************************/

/******************************************************************************************/
/*usart4*/
#define USART4_TX_GPIO_PORT                  GPIOC
#define USART4_TX_GPIO_PIN                   GPIO_PIN_10
#define USART4_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART4_RX_GPIO_PORT                  GPIOC
#define USART4_RX_GPIO_PIN                   GPIO_PIN_11
#define USART4_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART4_UX                            UART4
#define USART4_UX_IRQn                       UART4_IRQn
#define USART4_UX_IRQHandler                 UART4_IRQHandler
#define USART4_UX_CLK_ENABLE()               do{ __HAL_RCC_UART4_CLK_ENABLE(); }while(0)  /* USART1 ʱ��ʹ�� */

/******************************************************************************************/

/******************************************************************************************/
/*usart5*/
#define USART5_TX_GPIO_PORT                  GPIOC
#define USART5_TX_GPIO_PIN                   GPIO_PIN_12
#define USART5_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART5_RX_GPIO_PORT                  GPIOD
#define USART5_RX_GPIO_PIN                   GPIO_PIN_2
#define USART5_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

#define USART5_UX                            UART5
#define USART5_UX_IRQn                       UART5_IRQn
#define USART5_UX_IRQHandler                 UART5_IRQHandler
#define USART5_UX_CLK_ENABLE()               do{ __HAL_RCC_UART5_CLK_ENABLE(); }while(0)  /* USART1 ʱ��ʹ�� */

/******************************************************************************************/
 

#define DMA_RX_BUFER_SIZE 300
#define RXBUFFERSIZE      1
void blue_printf(const char *format, ...);
void usart1_init(uint32_t bound);                /* ���ڳ�ʼ������ */
void usart2_init(uint32_t bound);                /* ���ڳ�ʼ������ */
void usart3_init(uint32_t bound);                /* ���ڳ�ʼ������ */
void usart4_init(uint32_t bound);                /* ���ڳ�ʼ������ */
void usart5_init(uint32_t bound);                /* ���ڳ�ʼ������ */
void uart_dma_idle_start(void);
void uart_blue_idle_start(void);
void uart_port_init(void);
UART_HandleTypeDef *getusartHandle(uint8_t num);
uint8_t uart_transmit_it(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);
#endif


