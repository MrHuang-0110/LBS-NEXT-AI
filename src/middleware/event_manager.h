#ifndef __EVENT_MANAGER_H
#define __EVENT_MANAGER_H

#include "stdbool.h"
#include "string.h"
#include "sys.h"

#define EVENT_MAX 16

typedef struct {
		char name[16];
    uint32_t id;
    uint32_t threshold_ticks;
    uint32_t accum_ticks;
    void (*callback)(void *arg);
    void *arg;
    bool enabled;
}Event_t;

typedef struct{
  char name[32];
	uint32_t ms;
	void (*callback)(void*);
	void *arg;
}EVENT_MANAGER;
 
uint32_t event_register(char *name,uint32_t threshold_ms, void (*callback)(void *), void *arg);
void event_schedlucer(uint32_t cpu_tick);
void set_event_threshold_ticks(char *name,uint32_t threshold_ms);
void set_event_disable(char *name);
void set_event_enable(char *name);
void create_event_manger(EVENT_MANAGER *event);
 
#endif
