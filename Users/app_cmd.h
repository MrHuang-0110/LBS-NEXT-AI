#ifndef APP_CMD_H
#define APP_CMD_H

#include <stdint.h>
#include "protocol.h"

void AppCmd_Init(void);
void AppCmd_SyncBtFromModule(void);
void AppCmd_PollUsb(void);
void AppCmd_PollBt(void);
void app_cmd_poll_callback(void *arg);
uint8_t AppCmd_IsYmodemActive(void);
void AppMonitor_SetUploadPaused(uint8_t paused);
uint8_t AppMonitor_IsUploadPaused(void);
void busDataparsing(_AGREEMENT *frame, void (*port_transerf_data)(void *data, uint16_t length));

/* 蓝牙模块属性缓存（供 monitor 上报用）*/
const char *AppBt_GetName(void);
const char *AppBt_GetAdvData(void);
void AppBt_SetName(const char *name);
void AppBt_SetAdvData(const char *hex);

#endif