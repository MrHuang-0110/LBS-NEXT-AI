#ifndef __MOTOR_H
#define __MOTOR_H
#include "btim.h"
#include "malloc.h"
 #include "delay.h"
#include "device_pool.h"

#define DEVICE_MOTOR_ID 0xA1

#define PORT_MOTOR_A 0x04
#define PORT_MOTOR_B 0x05
#define PORT_MOTOR_C 0x06
#define PORT_MOTOR_D 0x07
 

typedef enum{ 
  SILD_STOP,
	BREAK_STOP 		
}MOTOR_STATE;

typedef struct {
	SensorBase base;
	
	MOTOR_STATE stop_mode;/*停止模式*/
	uint8_t duty;/*功率*/
}DEV_MOTOR;

enum{
 LEFT_MOTOR_NEGATION = 0,
 RIGHT_MOTOR_NEGATION,
 DOUBLE_MOTOR_NEGATION,
 DOUBLE_MOTOR_DEFAULT
};

 
DEV_MOTOR *read_motor(void *self);
DEV_MOTOR *create_motor(void);

void motor_stop(uint8_t id);
void motor_stopToMode(uint8_t id,uint8_t mode);
void motor_set_pwm(uint8_t id,int pwm);
void cloase_all_motor(void);
void motor_delay_exit(uint32_t tick);
void set_motor_dir(float *left_duty,float *right_duty,int state);

// PID巡线控制函数
// 参数说明:
// left_gray, right_gray: 左右灰度传感器值（建议使用归一化值0-1000）
// base_power_left, base_power_right: 左右电机基础功率（0-100范围）
// Kp, Ki, Kd: PID参数
// final_power_left, final_power_right: 输出调整后的电机功率
// 注意：函数内部使用静态变量存储积分项和上次误差，仅适用于单路巡线
void pid_line_follow(float left_gray, float right_gray,
                     int base_power_left, int base_power_right,
                     float Kp, float Ki, float Kd);

 

// 重置PID状态（用于重新开始控制）
void pid_line_follow_reset(void);
#endif
