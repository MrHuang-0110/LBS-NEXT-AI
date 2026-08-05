#ifndef __ULTRASION_H
#define __ULTRASION_H

#include "sys.h"
#include "stdbool.h"
#include "device_pool.h"

typedef struct
{ 
	 SensorBase base;
   int  cm,dt;
}DEV_ULTRASION;


DEV_ULTRASION *read_ultrasion(void *self);
void refsh_ultrasion(void* self, void* data);
SensorBase *create_ultrasion(void);
void destroy_ultrasion(SensorBase *sensor);
#endif
