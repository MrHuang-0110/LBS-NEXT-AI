#ifndef APP_PIKA_RUNTIME_H
#define APP_PIKA_RUNTIME_H

#include <stdint.h>

typedef enum
{
    APP_PIKA_STATE_OFF = 0,
    APP_PIKA_STATE_READY,
    APP_PIKA_STATE_RUNNING,
} AppPikaState_t;

int AppPika_LoadBytecode(const uint8_t *data, uint32_t len);
int AppPika_Start(void);
int AppPika_Stop(void);
void AppPika_OnKeyToggle(void);
AppPikaState_t AppPika_GetState(void);
uint8_t AppPika_HasBytecode(void);
int AppPika_IsStopRequested(void);
void AppPika_CheckAbort(void);

#endif
