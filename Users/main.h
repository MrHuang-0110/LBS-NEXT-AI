#ifndef __MAIN_H
#define __MAIN_H

#include "sys.h"
#include "usart.h"
#include "delay.h"
#include "malloc.h"
#include "btim.h"
#include "led.h"
#include "drv_led.h"
#include "key.h"
#include "iic.h"
#include "spi.h"
#include "adc.h"
#include "wdg.h"
#include "event_manager.h"
#include "blue.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"
#include "protocol.h"
#include "lbsfilemanager.h"
#include "event_manager.h"
#include "json-maker.h"
#include "device_pool.h"
#include "motor.h"
#include "beep.h"
#define APP_START_ADDR      0x08008000

/* PC5：电源保持，拉高维持供电（与 Boot PWR_CTRL 一致） */
#define APP_PWR_CTRL_GPIO_PORT      GPIOC
#define APP_PWR_CTRL_GPIO_PIN       GPIO_PIN_5

#endif
