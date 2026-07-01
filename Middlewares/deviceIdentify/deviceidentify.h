#ifndef __DEVICEIDENTIFY_H
#define __DEVICEIDENTIFY_H

#include "./SYSTEM/sys/sys.h"

 
 

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
