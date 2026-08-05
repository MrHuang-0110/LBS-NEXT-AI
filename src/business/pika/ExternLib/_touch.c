#include "touch.h"
#include "_touch.h"
 

pika_float _touch_state(PikaObj *self, int port)
{ 
  DEV_TOUCH *touch = read_touch((SensorBase *)getHubBase(port));
	
	if(touch == NULL)
		  return 0;
	
  return (pika_float)touch->touchState;
}

 
