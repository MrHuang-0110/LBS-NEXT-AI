#include "ultrasion.h"
#include "_ultrasion.h"


int _ultrasion_cmp_value(PikaObj *self, int port, char* judgment, pika_float value)
{ 
 DEV_ULTRASION *ultrasion = read_ultrasion((SensorBase *)getHubBase(port));
   if(ultrasion == NULL)
		   return 0;
	 
		if(strcmp(judgment,"<") == 0)
	{
    if(ultrasion->cm < value)
         return 1;
	   else
	   	   return 0;
	}
	else if(strcmp(judgment,">") == 0)
	{

			if(ultrasion->cm > value)
					return 1;
			else
					return 0;
	}
	else if(strcmp(judgment,"=") == 0)
	{
       if((int)ultrasion->cm == (int)value)
            return 1;
	     else
	   	  	  return 0;
	}
	return 0;
}
pika_float _ultrasion_value(PikaObj *self, int port)
{ 
   DEV_ULTRASION *ultrasion = read_ultrasion((SensorBase *)getHubBase(port));
	 if(ultrasion == NULL)
		  return 0;
	
	  return ultrasion->cm;
}

