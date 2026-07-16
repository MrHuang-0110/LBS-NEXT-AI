#include "_os.h"
#include "btim.h"
#include "PikaVM.h"
#include "math.h"
#include "./SYSTEM/delay/delay.h"
pika_float _os_timer(PikaObj *self)
{
  return getUserCPUTick();
}
void _os_resetTimer(PikaObj *self)
{
  resetUserCPUTick();
}
void _os_sleep_s(PikaObj *self, pika_float tick)
{
	uint32_t total_ms = (uint32_t)(fabs(tick) * 1000.0f);
    if (total_ms == 0) return;

    uint32_t start_ticks = getTim6Tick();
    uint32_t target_ticks = start_ticks + total_ms;

    pika_GIL_EXIT();

    while (getTim6Tick() < target_ticks) {
        if (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT) {
            break;
        }
        /* 在 sleep 期间轮询 hook，保持监控上报和命令响应不中断 */
        pika_hook_instruct();
        delay_ms(1);
    }
    pika_GIL_ENTER();
}
void _os_stop_exit(PikaObj *self)
{
	 extern void __exitpython(void);
   __exitpython();
}
int _os_get_port_linke(PikaObj *self, int port)
{
	 extern uint8_t GetHubLinkeDeviceId(uint8_t id);
   return GetHubLinkeDeviceId(port);
}