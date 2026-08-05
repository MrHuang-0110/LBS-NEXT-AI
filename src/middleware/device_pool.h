#ifndef __DEVICE_POOL_H
#define __DEVICE_POOL_H

#include "sys.h"

/* 设备类型 ID（收口自原 deviceidentify.h / 各业务头，统一在 middleware 定义，
 * 供 device_pool 注册表与业务/协议层共用） */
#define DEVICE_MOTOR_ID       0xA1
#define DEVICE_COLOR_ID       0xA2
#define DEVICE_ULTRASION_ID   0xA3
#define DEVICE_TOUCH_ID       0xA4
#define DEVICE_BLUE_ID        0xAF

/* 电机端口（收口自 motor.h，device_pool 内部绑定电机使用） */
#define PORT_MOTOR_A 0x04
#define PORT_MOTOR_B 0x05
#define PORT_MOTOR_C 0x06
#define PORT_MOTOR_D 0x07

typedef struct {
   int  type;	 
   char name[16];
   void (*setParam)(void* self, void *data);
}SensorBase;


typedef struct{ 
	SensorBase *sensors;
	uint8_t hub_id;
  uint8_t LinkeDeviceID;
	uint16_t portTimeOutTick;
}_DEVICE_HUB;

/* 设备注册表：业务层在启动时注册 create/set_param/destroy，
 * device_pool 不再 include 业务头（切断 middleware→business 倒挂） */
typedef SensorBase *(*DeviceCreateFn)(void);
typedef void (*DeviceSetParamFn)(void *self, void *data);
typedef void (*DeviceDestroyFn)(SensorBase *sensor);

void DevicePool_Register(uint8_t id, DeviceCreateFn create, DeviceSetParamFn set_param, DeviceDestroyFn destroy);
SensorBase *DevicePool_Create(uint8_t id, _DEVICE_HUB *hub);
void DevicePool_Destroy(SensorBase *sensor);
/* 返回最近一次 DevicePool_Create 对应的 hub->hub_id（供业务 create 包装查询端口号） */
uint8_t DevicePool_GetCurrentHubId(void);

SensorBase *getHubBase(uint8_t id); 
extern _DEVICE_HUB hub_port[9];

void destroy_device(SensorBase *sensor);
void identify_and_bind(_DEVICE_HUB *manager, uint8_t id);
void set_sensor_parameter(SensorBase* sensor,void *param);
SensorBase *HubBase_And_identify(uint8_t id,uint8_t sourceId);
uint8_t GetHubLinkeDeviceId(uint8_t id);
void HubBase_Scan_TimeOut(void);
void init_identify_dev(void);
#endif
