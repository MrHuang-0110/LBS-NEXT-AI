#ifndef __COLOR_H
#define __COLOR_H
#include "sys.h"
#include "stdbool.h"
#include "deviceidentify.h"
typedef int FRESULT;
#define FR_OK 0
#define DEVICE_COLOR_ID          0xA2
#define DEFAULT_THRESHOLD_VALUE  1000

typedef struct{
	 uint8_t calibrated;        // 是否已校准: 0-未校准, 1-已校准
	 uint8_t calibrating;       // 正在校准: 0-否, 1-是
	 uint16_t thresholdValue;   // 校准阈值
	 uint32_t cal_start_time;   // 校准开始时间(毫秒)
	 uint32_t cal_time_ms;      // 校准时间(毫秒)
	 int cal_min;               // 校准最小值 (原始ADC值)
	 int cal_max;               // 校准最大值 (原始ADC值)
	 
}CALIBRATION;
typedef struct
{
	 SensorBase base;
	 int reg_lux,lux,lux_state;
	 CALIBRATION color_calibation;
}DEV_COLOR;

void refsh_color(void* self, void* data);
void read_color_cfg(void* self,uint8_t hub_id);
FRESULT write_color_cfg(int port,CALIBRATION *cal);

DEV_COLOR *read_color(void *self);
DEV_COLOR *create_color(void);
 
void color_calibrate(DEV_COLOR *color1,DEV_COLOR *color2,uint32_t time_ms);
#endif
