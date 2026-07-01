#ifndef __BLUE_H
#define __BLUE_H
#include "stdbool.h"
#include "deviceidentify.h"

#define PORT_BLUE    0x08
#define DEVICE_BLUE_ID      0xAF

typedef struct{
  int blue_init_state,on_off;
}BLUE_CFG;

typedef struct{ 
	SensorBase base;
	UART_HandleTypeDef *huart;
  bool is_resh_flag;
	bool is_off_on;
	uint8_t remoteValue[10];
	char    at_cmd_bufer[128];
	BLUE_CFG cfg;
}DEV_BLUE;

DEV_BLUE *read_blue(void *self);
DEV_BLUE *create_blue(void);
void refsh_blue(void* self, void* data);
void blue_init(void);
void blue_force_reinit(void);
void blue_set_on(void);
void blue_set_off(void);
void blue_logo_blinke(void);
#endif
