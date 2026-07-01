
#include "./BSP/LED/led.h"

#include "key.h"
#include "adc.h"
#include "stdio.h"
#include "string.h"
#include "blue.h"
/**
 * @brief       ��ʼ��LED���IO��, ��ʹ��ʱ��
 * @param       ��
 * @retval      ��
 */
void led_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
	  
	  #if WIALL_HARDWARE_ENABLE
    LED0_GPIO_CLK_ENABLE();                                 /* LED0ʱ��ʹ�� */
    LED1_GPIO_CLK_ENABLE();                                 /* LED1ʱ��ʹ�� */
    LED2_GPIO_CLK_ENABLE();                                 /* LED2ʱ��ʹ�� */
	
    gpio_init_struct.Pin = LED0_GPIO_PIN;                   /* LED0���� */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* ������� */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* ���� */
    
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* ���� */
    HAL_GPIO_Init(LED0_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��LED0���� */

    gpio_init_struct.Pin = LED1_GPIO_PIN;                   /* LED1���� */
    HAL_GPIO_Init(LED1_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��LED1���� */

    /* ��ˮ�� PC0~PC3������״̬ PC15 �� DrvLed_Init ��ʼ�����˴�����ʼ����ص� PB0/PB1 */
    LED0(1);
    LED1(1);
	  #else
    LED0_GPIO_CLK_ENABLE();                                 /* LED0ʱ��ʹ�� */
    LED1_GPIO_CLK_ENABLE();                                 /* LED1ʱ��ʹ�� */

    gpio_init_struct.Pin = LED0_GPIO_PIN;                   /* LED0���� */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;            /* ������� */
    gpio_init_struct.Pull = GPIO_PULLUP;                    /* ���� */
    
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* ���� */
    HAL_GPIO_Init(LED0_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��LED0���� */

    gpio_init_struct.Pin = LED1_GPIO_PIN;                   /* LED1���� */
    HAL_GPIO_Init(LED1_GPIO_PORT, &gpio_init_struct);       /* ��ʼ��LED1���� */
	
    LED0(1);                                                /* �ر� LED0 */
    LED1(1);                                                /* �ر� LED1 */
	  #endif
}

 
#if WIALL_HARDWARE_ENABLE
void led3_event_callback(void *arg)
{ 
	extern void usb_printf(char *fmt, ...);
 DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(PORT_BLUE));
 
			if(blue->is_off_on)
			{ 
			  if(BLUE_STA)
					LED2(0);
				else{ 
					LED2_TOGGLE();
				}
					 
			}	 
			else{ 
			  LED2(1);
			}
}
#endif
