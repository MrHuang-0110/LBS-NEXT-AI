#ifndef __TOUCH_H
#define __TOUCH_H
#include "./SYSTEM/sys/sys.h"
#include "stdbool.h"
#include "deviceidentify.h"

#define DEVICE_TOUCH_ID 0xA4

typedef struct
{ 
	 SensorBase base;
   int touchState;
}DEV_TOUCH;


DEV_TOUCH *read_touch(void *self);
void refsh_touch(void* self, void* data);
DEV_TOUCH *create_touch(void);
#endif
