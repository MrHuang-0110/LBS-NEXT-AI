#include "motor.h"
#include "PikaVM.h"
 
extern void usb_printf(char *fmt, ...);
extern uint8_t os_is_port_serial_mode(uint8_t motor_id);
void motor_stopToMode(uint8_t id,uint8_t mode)
{ 
 	switch(mode)
	{
	  case SILD_STOP:
			pwm_stop_slide(id);
		break;
		
		case BREAK_STOP:
			pwm_stop_breaking(id);
		break;
	}  
}
void motor_stop(uint8_t id)
{ 
  DEV_MOTOR *motor = (DEV_MOTOR*)getHubBase(id);
	switch((uint8_t)motor->stop_mode)
	{
	  case SILD_STOP:
			pwm_stop_slide(id);
		break;
		
		case BREAK_STOP:
			pwm_stop_breaking(id);
		break;
	}
}
DEV_MOTOR *create_motor(void)
{ 
    DEV_MOTOR *motor = mymalloc(SRAMIN,sizeof(DEV_MOTOR));
    if(motor == NULL) return NULL;
    
    *motor = (DEV_MOTOR){
        .base = {
            .type = DEVICE_MOTOR_ID,
            .name = "motor"
        },
				.duty = 50,
				.stop_mode = SILD_STOP,
    };
		
    return motor; 
}
DEV_MOTOR *read_motor(void *self)
{
   DEV_MOTOR *mt = (DEV_MOTOR*)self;
	 return mt;     
}
void motor_set_pwm(uint8_t id,int pwm)
{ 
   pwm_set_output(id,pwm);
}

void motor_delay_exit(uint32_t tick)
{ 
  uint32_t total_ms = (uint32_t)(tick);  
    if (total_ms == 0) return;
    
    uint32_t start_ticks = HAL_GetTick();
    uint32_t target_ticks = start_ticks + total_ms;
    
    pika_GIL_EXIT();
    
    while (HAL_GetTick() < target_ticks) {
        if (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT) {
            break;
        }
        uint32_t remaining_ticks = target_ticks - HAL_GetTick();
        uint32_t delay_ticks = (remaining_ticks > 1) ? 
                                1 : remaining_ticks;
        
        if (delay_ticks > 0) {
            delay_ms(delay_ticks);
        } else {
            break;
        }
    }   
    pika_GIL_ENTER();
}

void set_motor_dir(float *left_duty,float *right_duty,int state)
{ 
    switch(state)
		{
		  case LEFT_MOTOR_NEGATION:*left_duty*=(-1);break;
			case RIGHT_MOTOR_NEGATION:*right_duty*=(-1);break;
			case DOUBLE_MOTOR_NEGATION:*left_duty*=(-1);*right_duty*=(-1);break;
			case DOUBLE_MOTOR_DEFAULT:*left_duty*=(-1);break;
		}
}

void cloase_all_motor(void)
{
    if (!os_is_port_serial_mode(PORT_MOTOR_A))
        motor_stopToMode(PORT_MOTOR_A, BREAK_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_B))
        motor_stopToMode(PORT_MOTOR_B, BREAK_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_C))
        motor_stopToMode(PORT_MOTOR_C, BREAK_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_D))
        motor_stopToMode(PORT_MOTOR_D, BREAK_STOP);

    delay_ms(30);

    if (!os_is_port_serial_mode(PORT_MOTOR_A))
        motor_stopToMode(PORT_MOTOR_A, SILD_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_B))
        motor_stopToMode(PORT_MOTOR_B, SILD_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_C))
        motor_stopToMode(PORT_MOTOR_C, SILD_STOP);
    if (!os_is_port_serial_mode(PORT_MOTOR_D))
        motor_stopToMode(PORT_MOTOR_D, SILD_STOP);

    delay_ms(30);

    for (uint8_t i = 4; i < 8; i++)
    {
        if (os_is_port_serial_mode(i)) continue;
        DEV_MOTOR *motor = (DEV_MOTOR *)getHubBase(i);
        motor->stop_mode = SILD_STOP;
        motor->duty = 50;
    }
}
