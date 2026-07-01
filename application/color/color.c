#include "color.h"
#include "drv_flash_storage.h"
#include "malloc.h"
#include "string.h"
#include "stdio.h"
#include "protocol.h"
#include "usbd_cdc_interface.h"
#include "key.h"
#include <math.h>
#include "btim.h"
 


static int map_to_range(int raw, DEV_COLOR *color)
{
	  if(color->color_calibation.thresholdValue == 0)
				color->color_calibation.thresholdValue = DEFAULT_THRESHOLD_VALUE/2.0f;
		
    if (color->color_calibation.calibrated)
    {
        // 使用校准范围映射
        int range = color->color_calibation.cal_max - color->color_calibation.cal_min;
        if (range <= 0) range = 1; // 防止除零
        int mapped = (raw - color->color_calibation.cal_min) * DEFAULT_THRESHOLD_VALUE / range;
        if (mapped < 0) mapped = 0;
        if (mapped > DEFAULT_THRESHOLD_VALUE) mapped = DEFAULT_THRESHOLD_VALUE;
        return mapped;
    }
    else
    {
        return raw;
    }
}

static void update_color_value(DEV_COLOR *color)
{
    color->lux = map_to_range(color->reg_lux, color);
    //color->lux_state = (color->lux < (color->color_calibation.thresholdValue / 2.0f)) ? 1 : 0; // 1:黑线, 0:白线
	  color->lux_state = (color->lux < color->color_calibation.thresholdValue) ? 1 : 0;
}

void color_calibrate(DEV_COLOR *color1,DEV_COLOR *color2,uint32_t time_ms)
{
    // 初始化校准参数
	  if(color1!=NULL)
		{ 
			color1->color_calibation.calibrating = 1;
			color1->color_calibation.cal_start_time = HAL_GetTick(); // 假设可用
			color1->color_calibation.cal_time_ms = time_ms;
			color1->color_calibation.cal_min = 4095;  // 初始最小值设为最大
			color1->color_calibation.cal_max = 0;     // 初始最大值设为最小
			color1->color_calibation.calibrated = 0;  // 重置校准标志		   
		}
 
    if(color2!=NULL)
		{ 
			color2->color_calibation.calibrating = 1;
			color2->color_calibation.cal_start_time = HAL_GetTick(); // 假设可用
			color2->color_calibation.cal_time_ms = time_ms;
			color2->color_calibation.cal_min = 4095;  // 初始最小值设为最大
			color2->color_calibation.cal_max = 0;     // 初始最大值设为最小
			color2->color_calibation.calibrated = 0;  // 重置校准标志 
		}
}

DEV_COLOR *read_color(void *self)
{ 
	 DEV_COLOR *mt = (DEV_COLOR*)self;
	 return mt;   
}

void refsh_color(void* self, void* data)
{
    DEV_COLOR *mt = (DEV_COLOR*)self;

    _AGREEMENT *_fd = (_AGREEMENT *)data;

    int original_lux_state;
    sscanf((const char*)_fd->data,"%d,%d",&mt->reg_lux,&original_lux_state);

    if (mt->color_calibation.calibrating)
    {
        if (mt->reg_lux < mt->color_calibation.cal_min) mt->color_calibation.cal_min = mt->reg_lux;
        if (mt->reg_lux > mt->color_calibation.cal_max) mt->color_calibation.cal_max = mt->reg_lux;

        if ((HAL_GetTick() - mt->color_calibation.cal_start_time) >= mt->color_calibation.cal_time_ms)
        {
            mt->color_calibation.calibrated = 1;
            mt->color_calibation.calibrating = 0;
        }
    }
		extern volatile bool start_pauto;
    if (!mt->color_calibation.calibrated || start_pauto)
    {
			  mt->lux = mt->reg_lux;
        mt->lux_state = original_lux_state;
    }
		else
		{ 
			update_color_value(mt);
		}
}

DEV_COLOR *create_color(void)
{ 
 	 DEV_COLOR *color = mymalloc(SRAMIN,sizeof(DEV_COLOR));
	 memset(color,0,sizeof(DEV_COLOR));
	 color->base.type = DEVICE_COLOR_ID;
	 memset(color->base.name,0,sizeof(color->base.name));
	 strcpy(color->base.name,"color"); 
	 color->color_calibation.thresholdValue = DEFAULT_THRESHOLD_VALUE/2.0f;
	 return color;
}

void read_color_cfg(void* self,uint8_t hub_id)
{ 
	 DEV_COLOR *mt = (DEV_COLOR*)self;
	 if(mt == NULL)return;
	 if (DrvFlashStorage_ReadColorCalib(hub_id, &mt->color_calibation) != 0)
	 {
	     memset(&mt->color_calibation, 0, sizeof(CALIBRATION));
	     mt->color_calibation.thresholdValue = DEFAULT_THRESHOLD_VALUE / 2U;
	 }
}

FRESULT write_color_cfg(int port,CALIBRATION *cal)
{ 
	 if ((cal == NULL) || (port < 0) || ((uint8_t)port >= HUB_PORT_MAX_NUM))
	 {
	     return FR_OK;
	 }
	 if (DrvFlashStorage_WriteColorCalib((uint8_t)port, cal) == 0)
	 {
	     return FR_OK;
	 }
   return FR_OK;
}
 
