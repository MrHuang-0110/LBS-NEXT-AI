#ifndef __CMD_H
#define __CMD_H
#include "frame.h"
#include <stdint.h>
void Cmd_Init(void);
void Cmd_PollCallback(void *arg);
void Cmd_ProcessFrame(_AGREEMENT *frame);
void Cmd_RegisterAction(uint8_t index, void (*action)(_AGREEMENT *frame));
void Cmd_RegisterLineHandler(void (*handler)(const char *line, uint8_t port));
void Cmd_SetYmodemActive(uint8_t on);
uint8_t Cmd_IsYmodemActive(void);
void Cmd_ProcessPendingLines(void);
extern volatile uint8_t g_cmd_pending;
#endif
