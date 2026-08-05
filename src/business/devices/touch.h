#ifndef __TOUCH_H
#define __TOUCH_H
#include "sys.h"
#include "stdbool.h"
#include "device_pool.h"

typedef struct
{ 
	 SensorBase base;
   int touchState;
}DEV_TOUCH;


DEV_TOUCH *read_touch(void *self);
void refsh_touch(void* self, void* data);
SensorBase *create_touch(void);
void destroy_touch(SensorBase *sensor);
#endif
