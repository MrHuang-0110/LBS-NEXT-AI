#include "_motor.h"
#include "motor.h"
#include "PikaVM.h"
#include "stdbool.h"
#include "math.h"
#include "key.h"

typedef struct{
  uint8_t id[2];
	int state;
	uint8_t mov_stop;
	int     mov_duty;
}DoubleMotor;

static DoubleMotor d_motor;
static float pid_integral = 0.0f;          // 积分项
static float pid_prev_error = 0.0f;        // 上次误差
static uint32_t pid_prev_time = 0;         // 上次调用时间
static bool pid_first_call = true;         // 首次调用标志
extern void usb_printf(char *fmt, ...);

#define PID_INTEGRAL_LIMIT 500.0f        // 积分项限幅
#define PID_OUTPUT_LIMIT 200.0f            // PID输出限幅（相对调整量）

static void double_motor_dir_changer_single(char *dir, float *duty1, float *duty2) {
    if (dir == NULL || duty1 == NULL || duty2 == NULL) return;
    if (strcmp(dir, "retreat") == 0) {
        *duty1 = -*duty1;
        *duty2 = -*duty2;
    } else if (strcmp(dir, "right") == 0) {
        *duty2 = -*duty2;
    } else if (strcmp(dir, "left") == 0) {
        *duty1 = -*duty1;
    }
}

static DEV_MOTOR *check_motor_line(int port)
{ 	
	return port>8?NULL:(DEV_MOTOR*)getHubBase(port);
}

void pid_line_follow_reset(void)
{
    pid_integral = 0.0f;
    pid_prev_error = 0.0f;
    pid_prev_time = 0;
    pid_first_call = true;
} 

void	pid_line_follow(float left_gray, float right_gray,
                     int base_power_left, int base_power_right,
                     float Kp, float Ki, float Kd)
{

    float error = (left_gray - right_gray)/10.0f;
   
    uint32_t current_time = HAL_GetTick();
    float dt = 0.0f;

    if (pid_first_call) {
        dt = 0.0f;
        pid_prev_error = error;
        pid_prev_time = current_time;
        pid_first_call = false;
    } else {
        dt = (float)(current_time - pid_prev_time) / 1000.0f;
        if (dt <= 0.0f || dt > 1.0f) {
            dt = 0.01f;
        }
    }
 
    float proportional = Kp * error;
     
    pid_integral += error * dt;
	  
    if (pid_integral > PID_INTEGRAL_LIMIT) pid_integral = PID_INTEGRAL_LIMIT;
    if (pid_integral < -PID_INTEGRAL_LIMIT) pid_integral = -PID_INTEGRAL_LIMIT;
    float integral = Ki * pid_integral;
     
    float derivative = 0.0f;
    if (dt > 0.0f && Kd != 0.0f) {
        derivative = Kd * (error - pid_prev_error) / dt;
    }
   
    float adjustment = proportional + integral + derivative;
 
    if (adjustment > PID_OUTPUT_LIMIT) adjustment = PID_OUTPUT_LIMIT;
    if (adjustment < -PID_OUTPUT_LIMIT) adjustment = -PID_OUTPUT_LIMIT;

    pid_prev_error = error;
    pid_prev_time = current_time;
    
    float left_power = abs(base_power_left) + adjustment;
    float right_power = abs(base_power_right) - adjustment;

    if (left_power > 100.0f) left_power = 100.0f;
    if (left_power <-100.0f) left_power = -100.0f;
    if (right_power > 100.0f) right_power = 100.0f;
    if (right_power < -100.0f) right_power =-100.0f;
    
		if(base_power_left<0)left_power = -left_power;
		if(base_power_right<0)right_power = -right_power;
		
	  motor_set_pwm(d_motor.id[0],left_power);
	  motor_set_pwm(d_motor.id[1],right_power);
}

void _motor_run_for_power_seconds(PikaObj *self, int port, pika_float duty, pika_float degrees)
{ 
   DEV_MOTOR *motor = check_motor_line(port);
	 if(motor == NULL)return;
	
	 if(degrees<0)
			duty = -duty;
	 motor_set_pwm(port,duty);
	
	 motor_delay_exit((uint32_t)(fabs(degrees) * 1000.0f));
	
	 motor_stopToMode(port,BREAK_STOP);  
}

void _motor_run_power(PikaObj *self, int port, pika_float duty)
{ 
   DEV_MOTOR *motor = check_motor_line(port);
	 if(motor == NULL)return;

   motor_set_pwm(port,duty);
}
void _motor_stop(PikaObj *self, int port)
{ 
  DEV_MOTOR *motor = check_motor_line(port);
	if(motor == NULL)return;
	 motor_stop(port);
}
void _motor_stop_module(PikaObj *self, int port, int stop)
{ 
  DEV_MOTOR *motor = check_motor_line(port);
	if(motor == NULL)return;   
 
	motor->stop_mode = stop;
}

void _motor_pair(PikaObj *self, int port1, int port2, int state)
{ 
   memset(&d_motor,0,sizeof(DoubleMotor));
	 
	 d_motor.id[0] = port1;
	 d_motor.id[1] = port2;
	
	 d_motor.mov_duty = 50;
	 d_motor.mov_stop = SILD_STOP;
	 d_motor.state = state;
}

void _motor_mov_set_stop_module(PikaObj *self, int mode)
{ 
   d_motor.mov_stop = mode;
}

void _motor_mov_dir_power_seconds(PikaObj *self, char* dir, pika_float duty, pika_float degrees)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
	
  float left_duty = duty,right_duty = duty;
	
	set_motor_dir(&left_duty,&right_duty,d_motor.state);	
	double_motor_dir_changer_single(dir,&left_duty,&right_duty);

	if(degrees<0){left_duty*=(-1);right_duty*=(-1);}
	
	motor_set_pwm(d_motor.id[0],(int)left_duty);
	motor_set_pwm(d_motor.id[1],(int)right_duty);
	
  motor_delay_exit((uint32_t)(fabs(degrees) * 1000.0f));
	
	motor_stopToMode(d_motor.id[0],BREAK_STOP);
	motor_stopToMode(d_motor.id[1],BREAK_STOP);		
}

void _motor_mov_dir_power(PikaObj *self, char* dir, pika_float duty)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
	
  float left_duty = duty,right_duty = duty;
	
	set_motor_dir(&left_duty,&right_duty,d_motor.state);	
	double_motor_dir_changer_single(dir,&left_duty,&right_duty);
	
	motor_set_pwm(d_motor.id[0],(int)left_duty);
	motor_set_pwm(d_motor.id[1],(int)right_duty);
}

void _motor_mov_stop(PikaObj *self)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
  
  motor_stopToMode(d_motor.id[0],d_motor.mov_stop);
	motor_stopToMode(d_motor.id[1],d_motor.mov_stop);
}

void	_motor_mov_for_power_seconds(PikaObj *self, pika_float duty1, pika_float duty2, pika_float degrees)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
	
  float left_duty = duty1,right_duty = duty2;
	set_motor_dir(&left_duty,&right_duty,d_motor.state);	
	
	if(degrees<0){left_duty*=(-1);right_duty*=(-1);}
	
	motor_set_pwm(d_motor.id[0],left_duty);
	motor_set_pwm(d_motor.id[1],right_duty);
	
  motor_delay_exit((uint32_t)(fabs(degrees) * 1000.0f));
	
	motor_stopToMode(d_motor.id[0],BREAK_STOP);
	motor_stopToMode(d_motor.id[1],BREAK_STOP);	
}

void _motor_mov_power(PikaObj *self, pika_float duty1, pika_float duty2)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
	
	float left_duty = duty1,right_duty = duty2;
	set_motor_dir(&left_duty,&right_duty,d_motor.state);	
	
	motor_set_pwm(d_motor.id[0],left_duty);
	motor_set_pwm(d_motor.id[1],right_duty);
}

void _motor_mov_find_line_init(PikaObj *self)
{ 
   pid_line_follow_reset();
}

void _motor_mov_find_line_run(PikaObj *self, pika_float gray_left, pika_float gray_right, pika_float power1, pika_float power2, pika_float kp, pika_float kd)
{ 
  DEV_MOTOR *mast_motor = check_motor_line(d_motor.id[0]);
	DEV_MOTOR *salve_motor = check_motor_line(d_motor.id[1]);
	if(mast_motor == NULL || salve_motor == NULL)return; 
	
	float left_duty = power1,right_duty = power2;
	set_motor_dir(&left_duty,&right_duty,d_motor.state);	
	
  pid_line_follow(gray_left,
									gray_right,
									left_duty,
									right_duty,
									kp,0,kd);
} 
 
void _motor_mov_set_advance_offset(PikaObj *self, int offset1, int offset2)
{ 
   write_advance_remote_cfg(offset1,offset2);
}
void _motor_mov_set_retreat_offset(PikaObj *self, int offset1, int offset2)
{ 
   write_retreat_remote_cfg(offset1,offset2);
}

 