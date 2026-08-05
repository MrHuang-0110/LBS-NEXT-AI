#ifndef __AT_H
#define __AT_H
#include <stdint.h>
void At_Init(void);
void At_SyncFromModule(void);
const char *At_GetName(void);
const char *At_GetAdvData(void);
void At_SetName(const char *name);
void At_SetAdvData(const char *hex);
void At_SendLine(const char *line);
uint8_t At_IsBusy(void);
#endif
