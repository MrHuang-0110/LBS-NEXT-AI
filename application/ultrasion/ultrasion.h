#ifndef __ULTRASION_H
#define __ULTRASION_H

#include "sys.h"
#include "stdbool.h"
#include "deviceidentify.h"

#define DEVICE_ULTRASION_ID      0xA3

typedef struct
{ 
	 SensorBase base;
   int  cm,dt;
}DEV_ULTRASION;


DEV_ULTRASION *read_ultrasion(void *self);
void refsh_ultrasion(void* self, void* data);
DEV_ULTRASION *create_ultrasion(void);
#endif
