#include "device_pool.h"
#include "malloc.h"


_DEVICE_HUB hub_port[9];

/* 设备注册表：按设备类型 ID 索引 create/set_param/destroy，
 * 业务层通过 DevicePool_Register 在启动时注册，本文件不再依赖任何业务头 */
static DeviceCreateFn  s_create[256];
static DeviceSetParamFn s_set_param[256];
static DeviceDestroyFn s_destroy[256];
static uint8_t s_cur_hub_id;   /* 最近一次 DevicePool_Create 的端口号 */

void DevicePool_Register(uint8_t id, DeviceCreateFn create, DeviceSetParamFn set_param, DeviceDestroyFn destroy)
{
    /* id 为 uint8_t，恒小于注册表长度 256，无需越界检查 */
    s_create[id] = create;
    s_set_param[id] = set_param;
    s_destroy[id] = destroy;
}

SensorBase *DevicePool_Create(uint8_t id, _DEVICE_HUB *hub)
{
    if (s_create[id] == NULL) { return NULL; }
    s_cur_hub_id = hub->hub_id;
    SensorBase *s = s_create[id]();
    /* 与重构前 switch 语义一致：无论 create 是否成功都记录 LinkeDeviceID */
    hub->LinkeDeviceID = id;
    if (s != NULL)
    {
        s->setParam = s_set_param[id];
    }
    return s;
}

void DevicePool_Destroy(SensorBase *sensor)
{
    if (sensor == NULL) { return; }
    if (s_destroy[sensor->type] != NULL) { s_destroy[sensor->type](sensor); }
}

uint8_t DevicePool_GetCurrentHubId(void)
{
    return s_cur_hub_id;
}

void destroy_device(SensorBase *sensor)
{
    DevicePool_Destroy(sensor);
}

void identify_and_bind(_DEVICE_HUB *manager, uint8_t id) 
{
    if(manager->sensors != NULL)
    {
        DevicePool_Destroy(manager->sensors);
        manager->sensors = NULL;
    }
    
    manager->sensors = DevicePool_Create(id, manager);
    /* read_color_cfg 特例由业务层 create 包装（create_color_cfg）等价处理 */
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
				  DevicePool_Destroy(hub_port[i].sensors);
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
