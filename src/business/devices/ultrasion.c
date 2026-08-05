#include "ultrasion.h"
#include "malloc.h"
#include "string.h"
#include "frame.h"
#include "stdio.h"
 

DEV_ULTRASION *read_ultrasion(void *self)
{
   DEV_ULTRASION *mt = (DEV_ULTRASION*)self;
	 return mt;
}
void refsh_ultrasion(void* self, void* data)
{ 
				DEV_ULTRASION *mt = (DEV_ULTRASION*)self;
			  _AGREEMENT *_fd = (_AGREEMENT *)data;
	      sscanf((const char*)_fd->data,"%d/%d",&mt->cm,&mt->dt);
}

SensorBase *create_ultrasion(void)
{ 
 				DEV_ULTRASION *ultrasion = mymalloc(SRAMIN,sizeof(DEV_ULTRASION));
				memset(ultrasion,0,sizeof(DEV_ULTRASION));
				ultrasion->base.type = DEVICE_ULTRASION_ID;
				memset(ultrasion->base.name,0,sizeof(ultrasion->base.name));
				strcpy(ultrasion->base.name,"ultrasion"); 
				ultrasion->cm = 0.0f;
 
				return (SensorBase *)ultrasion;		  
}

void destroy_ultrasion(SensorBase *sensor)
{
    DEV_ULTRASION *ultrasion = (DEV_ULTRASION *)sensor;
    myfree(SRAMIN, ultrasion);
}
