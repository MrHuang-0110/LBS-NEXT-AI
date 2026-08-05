#include "touch.h"
#include "malloc.h"
#include "string.h"
#include "frame.h"
#include "stdio.h"

 
DEV_TOUCH *read_touch(void *self)
{
   DEV_TOUCH *mt = (DEV_TOUCH*)self;
	 return mt;
}

void refsh_touch(void* self, void* data)
{ 
	      
				DEV_TOUCH *mt = (DEV_TOUCH*)self;
			  _AGREEMENT *_fd = (_AGREEMENT *)data;
				sscanf((const char*)_fd->data,"%d",&mt->touchState);
}

SensorBase *create_touch(void)
{ 
 				DEV_TOUCH *touch = mymalloc(SRAMIN,sizeof(DEV_TOUCH));
	      
				memset(touch,0,sizeof(DEV_TOUCH));
				touch->base.type = DEVICE_TOUCH_ID;
				memset(touch->base.name,0,sizeof(touch->base.name));
				strcpy(touch->base.name,"touch"); 
				touch->touchState = 0;
 
				return (SensorBase *)touch;		  
}

void destroy_touch(SensorBase *sensor)
{
    DEV_TOUCH *touch = (DEV_TOUCH *)sensor;
    myfree(SRAMIN, touch);
}
