#ifndef APP_CMD_H
#define APP_CMD_H

#include <stdint.h>
#include "frame.h"

void AppCmd_Init(void);
void AppCmd_SyncBtFromModule(void);
void AppMonitor_SetUploadPaused(uint8_t paused);
uint8_t AppMonitor_IsUploadPaused(void);

#endif
