#ifndef __EVENT_MANAGER_H
#define __EVENT_MANAGER_H

#include "stdbool.h"
#include "string.h"
#include "sys.h"

#define EVENT_MAX 16

/* 事件调度器阈值/计数均以 600Hz tick 为单位（1 tick ≈ 1.67ms）
 * 注：此处 tick 非 SysTick 周期，而是 event_schedlucer 每次被调用推进的
 *     计数步长（600Hz 时每 tick 约 1.67ms） */
typedef struct {
		char name[20];   /* 事件名缓冲：最长 "key_middle_event"(16字符)+'\0'，勿再改小 */
    uint32_t id;
    uint32_t threshold_ticks;   /* 触发阈值，单位 600Hz tick（1 tick ≈ 1.67ms） */
    uint32_t accum_ticks;       /* 累计计数，单位同上 */
    void (*callback)(void *arg);
    void *arg;
    volatile bool enabled;   /* 主循环 enable/disable，TIM6 ISR 读，须 volatile */
}Event_t;

typedef struct{
  char name[32];
	uint32_t ms;   /* 周期阈值，实际以 600Hz tick 计数（1 tick ≈ 1.67ms）；
	              * 字段名沿用历史名 ms，实为 tick 值，勿按毫秒解读 */
	void (*callback)(void*);
	void *arg;
}EVENT_MANAGER;
 
uint32_t event_register(char *name,uint32_t threshold_ticks, void (*callback)(void *), void *arg);
void event_schedlucer(uint32_t cpu_tick);
void set_event_threshold_ticks(char *name,uint32_t threshold_ticks);
void set_event_disable(char *name);
void set_event_enable(char *name);
void create_event_manger(EVENT_MANAGER *event);
 
#endif
