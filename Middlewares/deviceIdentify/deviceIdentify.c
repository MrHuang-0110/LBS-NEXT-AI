#include "deviceidentify.h"
#include "malloc.h"
#include "motor.h"
#include "blue.h"
#include "ultrasion.h"
#include "touch.h"
#include "color.h"
 

_DEVICE_HUB hub_port[9];

void destroy_device(SensorBase *sensor)
{
    if(sensor == NULL) return;
    
    switch(sensor->type)
    {
        case DEVICE_MOTOR_ID:
        {
					  DEV_MOTOR *motor = (DEV_MOTOR *)sensor;				 
						myfree(SRAMIN,motor);
					  motor = NULL;
            break;
        }
				case DEVICE_ULTRASION_ID:
				{				
					 DEV_ULTRASION *ultrasion = (DEV_ULTRASION *)sensor;				 
					 myfree(SRAMIN,ultrasion);		
					 ultrasion = NULL;
						break;	
				}
				case DEVICE_COLOR_ID:
				{		
					 DEV_COLOR *color = (DEV_COLOR *)sensor;				 
						myfree(SRAMIN,color);	
            color = NULL;					
						break;	
				}
				case DEVICE_TOUCH_ID:
				{ 
					 DEV_TOUCH *touch = (DEV_TOUCH *)sensor;				 
						myfree(SRAMIN,touch);
            touch = NULL;					
						break;
				}
				case DEVICE_BLUE_ID:
				{
					 DEV_BLUE *blue = (DEV_BLUE *)sensor;				 
						myfree(SRAMIN,blue);	
           blue = NULL;					
				   break;
				}
    }
}

void identify_and_bind(_DEVICE_HUB *manager, uint8_t id) 
{
    // ?????????????
    if(manager->sensors != NULL)
    {
        destroy_device(manager->sensors);
        manager->sensors = NULL;
    }
    
    // ?????????
    switch((uint8_t)id)
    {
        case DEVICE_MOTOR_ID:
        {
             manager->sensors = (SensorBase*)create_motor();
             if(manager->sensors != NULL)
                 manager->sensors->setParam = NULL;				
						 
						 manager->LinkeDeviceID = DEVICE_MOTOR_ID;
            break;
        }
				case DEVICE_ULTRASION_ID:
				{
             manager->sensors = (SensorBase*)create_ultrasion();
             if(manager->sensors != NULL)
                 manager->sensors->setParam = refsh_ultrasion;

							manager->LinkeDeviceID = DEVICE_ULTRASION_ID;
						break;	
				}
				case DEVICE_COLOR_ID:
				{		
					  
           manager->sensors = (SensorBase*)create_color();			   		
           /*??????????*/
					 read_color_cfg(manager->sensors,manager->hub_id);
             if(manager->sensors != NULL)
                 manager->sensors->setParam = refsh_color;

						manager->LinkeDeviceID = DEVICE_COLOR_ID;	
 						
						break;	
				}
				case DEVICE_TOUCH_ID:
				{ 
             manager->sensors = (SensorBase*)create_touch();
             if(manager->sensors != NULL)
                 manager->sensors->setParam = refsh_touch;	

						manager->LinkeDeviceID = DEVICE_TOUCH_ID;								 
						break;
				}
				case DEVICE_BLUE_ID:
				{
             manager->sensors = (SensorBase*)create_blue();
             if(manager->sensors != NULL)
                 manager->sensors->setParam = refsh_blue;		

							manager->LinkeDeviceID = DEVICE_BLUE_ID;								 
				  break;
				}
    }
}

void set_sensor_parameter(SensorBase* sensor,void *param)
{ 
    if (sensor == NULL) {     
				 
        return;
    }  
    if (sensor->setParam != NULL) {	 
        sensor->setParam(sensor, param);			   
    }
}
uint8_t GetHubLinkeDeviceId(uint8_t id)
{
   return hub_port[id].LinkeDeviceID;
}
SensorBase *getHubBase(uint8_t id)
{ 
    _DEVICE_HUB *g_device_manager = &hub_port[id];
    
    if(g_device_manager == NULL) {
        return NULL;
    }
    
    if(g_device_manager->sensors == NULL) {
        return NULL;
    }

    return g_device_manager->sensors;
}
SensorBase *HubBase_And_identify(uint8_t id,uint8_t sourceId)
{ 
 _DEVICE_HUB *g_device_manager = &hub_port[id];
    if(g_device_manager == NULL || 
			 g_device_manager->sensors == NULL) {
			  
				 g_device_manager->hub_id = id;
			 	identify_and_bind(g_device_manager,sourceId);		
    }
			 
		if(g_device_manager->sensors == NULL)return NULL;
		
		g_device_manager->portTimeOutTick = 250;/*500ms ?????????????????*/
    return  g_device_manager->sensors;
}

void HubBase_Scan_TimeOut(void)
{
   for(uint8_t i = 0; i < 2; i++)
	 { 
	    if(hub_port[i].portTimeOutTick > 0)
			{
			  hub_port[i].portTimeOutTick-=10;
				if(hub_port[i].portTimeOutTick == 0)
				{
					hub_port[i].LinkeDeviceID = 0;
					sys_intx_disable();
				  destroy_device(hub_port[i].sensors);
					hub_port[i].sensors = NULL;
					sys_intx_enable();
				}
			}
	 }
} 

void init_identify_dev(void)
{ 
	 identify_and_bind(&hub_port[PORT_MOTOR_A],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_B],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_C],DEVICE_MOTOR_ID);
	 identify_and_bind(&hub_port[PORT_MOTOR_D],DEVICE_MOTOR_ID);   
}
