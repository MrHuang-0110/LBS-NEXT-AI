#include "event_manager.h"
#include "lbsfilemanager.h"
 
Event_t event_list[EVENT_MAX];
uint8_t event_id[EVENT_MAX];
static uint32_t event_count = 0;
 
 

static bool find_event(char *name,uint32_t *event_id)
{ 
   uint32_t i;
	for(i = 0;i<EVENT_MAX;i++)
	{ 
	   if(strcmp(event_list[i].name,name) != 0)continue;
		 else
		 { 
		    *event_id = i;
			  break;
		 }
	}
	if(i == EVENT_MAX)return false;
  return true;	
}
uint32_t event_register(char *name,uint32_t threshold_ms, void (*callback)(void *), void *arg)
{
    if(event_count >= EVENT_MAX) return 0;
    
    uint32_t id = event_count + 1;
    
	  memset(event_list[event_count].name,0,sizeof(event_list[event_count].name));
	  strcpy(event_list[event_count].name,name);
	
    event_list[event_count].id = id;
    event_list[event_count].threshold_ticks = threshold_ms; 
    event_list[event_count].accum_ticks = 0;
    event_list[event_count].callback = callback;
    event_list[event_count].arg = arg;
    event_list[event_count].enabled = true;
    
    event_count++;
    return id;
}

void event_schedlucer(uint32_t cpu_tick)
{ 
      static volatile uint32_t last_cpu_tick = 0;
        for(uint32_t i = 0; i < event_count; i++)
        {
            if(!event_list[i].enabled) continue;
            
            event_list[i].accum_ticks += cpu_tick - last_cpu_tick;
            
            if(event_list[i].accum_ticks >= event_list[i].threshold_ticks)
            {
                if(event_list[i].callback != NULL)
                {
                    event_list[i].callback(event_list[i].arg);
                }
                event_list[i].accum_ticks = 0;
            }
        }
				last_cpu_tick = cpu_tick;   
}
void set_event_disable(char *name)
{
   uint32_t event_id;	
   if(find_event(name,&event_id))
	 { 
	    event_list[event_id].enabled = false;
	 }
}
void set_event_enable(char *name)
{
   uint32_t event_id;	
   if(find_event(name,&event_id))
	 { 
	    event_list[event_id].enabled = true;
	 }
}
void set_event_threshold_ticks(char *name,uint32_t threshold_ms)
{ 
    uint32_t event_id;	
   if(find_event(name,&event_id))
	 { 
	    event_list[event_id].threshold_ticks = threshold_ms;
		  event_list[event_id].enabled = true;
	 }
}

void create_event_manger(EVENT_MANAGER *event)
{ 
 
	   event_register(event->name,
										event->ms,
										event->callback,
										event->arg);
 
}