// monitor.h
#ifndef __MONITOR_H
#define __MONITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ??????????????
typedef enum {
    MONITOR_TYPE_INT,
    MONITOR_TYPE_STRING,
    MONITOR_TYPE_FLOAT,
    MONITOR_TYPE_BOOL,
    MONITOR_TYPE_JSON_OBJECT,  // ???JSON????
    MONITOR_TYPE_JSON_ARRAY    // JSON????
} MonitorDataType;

// ???????????????
typedef char* (*MonitorCallback)(void* context, size_t* remLen);

// ???????
typedef struct {
    const char* key;           // JSON????
    MonitorDataType type;      // ????????
    MonitorCallback callback;  // ??????????????
    void* context;            // ?????????????
    const char* description;   // ?????????????
} MonitorItem;

// ????????
typedef struct {
    uint8_t port;
    void* sensor;
    uint32_t device_id;
} DeviceInfo;

// ????????
typedef struct {
    MonitorItem* items;        // ?????????
    uint16_t item_count;       // ?????????
    uint16_t max_items;        // ?????????
    
    DeviceInfo* devices;       // ??????????
    uint8_t device_count;      // ???????
    
    char* json_buffer;         // JSON??????
    size_t buffer_size;        // ??????????
} MonitorManager;

// ?????????????
MonitorManager* monitor_init(char* buffer, size_t size);

// ???????
bool monitor_register_item(MonitorManager* manager, 
                          const char* key, 
                          MonitorDataType type,
                          MonitorCallback callback,
                          void* context,
                          const char* desc);

// ??????
bool monitor_register_device(MonitorManager* manager,
                            uint8_t port,
                            void* sensor,
                            uint32_t device_id);

// ?????????
bool monitor_generate_report(MonitorManager* manager);

// ???????
void monitor_output_report(MonitorManager* manager);
														
void monitor_call_back(void*arg);

/* 状态提供者：业务层（Pika 运行时）注册，返回 "run"/"stop" 字符串，
 * 使 protocol 层不依赖 business 的 Pika 状态 API（未注册时默认 "stop"） */
typedef const char *(*MonitorStateProvider)(void);
void Monitor_RegisterStateProvider(MonitorStateProvider cb);

/* 上传暂停：由 ymodem 等业务流程调用，monitor 上传期间暂停发送 */
void Monitor_SetUploadPaused(uint8_t paused);
uint8_t Monitor_IsUploadPaused(void);

#endif // __MONITOR_H