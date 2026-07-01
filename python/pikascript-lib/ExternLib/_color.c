#include "color.h"
#include "motor.h"
#include "matrix_port_stub.h"
#include "_color.h"
#include "PikaVM.h"

pika_float _color_cmp_lux(PikaObj *self, int port, char* judgment, pika_float value)
{ 
  DEV_COLOR *color = read_color((SensorBase *)getHubBase(port));
	
	if(color == NULL)
		  return 0;
	int resualt = 0;

   switch(*judgment)
	 { 
	   case '<':
			 resualt = (value < color->lux)?0:1;
		 break;
		 case '>':
			 resualt = (value > color->lux)?0:1;
		 break;
		 case '=':		   
			 resualt = (value == color->lux)?0:1;
		 break;
		 default:
			 resualt = 0;
		   break;
	 }

  return resualt;
}

pika_float _color_lux(PikaObj *self, int port)
{ 
  DEV_COLOR *color = read_color((SensorBase *)getHubBase(port));
	
	if(color == NULL)
		  return 0;
	
	return (pika_float)color->lux;
}

pika_float _color_lux_state(PikaObj *self, int port)
{ 
  DEV_COLOR *color = read_color((SensorBase *)getHubBase(port));
	
	if(color == NULL)
		  return 0;
	
	return (pika_float)color->lux_state;  
}

void _color_one_calibrate(PikaObj *self, int port, int timers)
{
  DEV_COLOR *color = read_color((SensorBase *)getHubBase(port));
	
	if(color == NULL)
		  return;
	int abs_timers = abs(timers);
  color_calibrate(color,NULL,(abs_timers * 1000));
	
    pika_GIL_EXIT();
    
    while ((!color->color_calibation.calibrated)) {
        if (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT)
            break;    
			char str[4];
      memset(str,0,sizeof(str));
      sprintf(str,"%d",abs_timers--);
			matrix_port_scroll_text(str,uvm_exit);
      motor_delay_exit(1000.0f);		    
    }   
    pika_GIL_ENTER();	
	  write_color_cfg(port,&color->color_calibation);
}

void _color_two_calibrate(PikaObj *self, int port1, int port2, int timers)
{ 
   DEV_COLOR *color1 = read_color((SensorBase *)getHubBase(port1));
	 DEV_COLOR *color2 = read_color((SensorBase *)getHubBase(port2));
	
	if(color1 == NULL || color2 == NULL)
		  return;
	 int abs_timers = abs(timers);
   color_calibrate(color1,color2,(abs_timers * 1000)); 
	 
    pika_GIL_EXIT();
    
    while ((!color1->color_calibation.calibrated && !color2->color_calibation.calibrated)) {
        if (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT)
            break;    
			char str[4];
      memset(str,0,sizeof(str));
      sprintf(str,"%d",abs_timers--);
			matrix_port_scroll_text(str,uvm_exit);
      motor_delay_exit(1000.0f);					
    }   
    pika_GIL_ENTER();	
		
		write_color_cfg(port1,&color1->color_calibation);
		write_color_cfg(port2,&color2->color_calibation);
}

void _color_set_color_threshold_value(PikaObj *self, int port, int value)
{ 
  DEV_COLOR *color = read_color((SensorBase *)getHubBase(port));
	if(color == NULL)
		  return;   
	
	color->color_calibation.thresholdValue = value;
	write_color_cfg(port,&color->color_calibation);
}