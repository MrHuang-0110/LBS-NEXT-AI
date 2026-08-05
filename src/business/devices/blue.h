#ifndef __BLUE_H
#define __BLUE_H
#include "stdbool.h"
#include "device_pool.h"

#define PORT_BLUE    0x08

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
SensorBase *create_blue(void);
void destroy_blue(SensorBase *sensor);
void refsh_blue(void* self, void* data);
void blue_init(void);
void blue_force_reinit(void);
void blue_set_on(void);
void blue_set_off(void);
void blue_logo_blinke(void);
#endif
