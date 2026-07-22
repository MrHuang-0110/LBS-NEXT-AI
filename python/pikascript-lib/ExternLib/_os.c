#include "_os.h"
#include "btim.h"
#include "PikaVM.h"
#include "math.h"
#include "motor.h"
#include "drv_led.h"
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

static uint8_t s_port_serial_mode[2]; /* 0:电机6, 1:电机7 */

uint8_t os_is_port_serial_mode(uint8_t motor_id)
{
    if (motor_id == PORT_MOTOR_C) return s_port_serial_mode[0];
    if (motor_id == PORT_MOTOR_D) return s_port_serial_mode[1];
    return 0U;
}

void _os_set_port_mode(PikaObj *self, int port, pika_float mode)
{
    uint8_t motor_id;
    uint8_t idx;

    if (port == 0)
    {
        motor_id = PORT_MOTOR_C;
        idx = 0U;
    }
    else if (port == 1)
    {
        motor_id = PORT_MOTOR_D;
        idx = 1U;
    }
    else
    {
        return;
    }

    s_port_serial_mode[idx] = ((int)mode == 1) ? 1U : 0U;

    if ((int)mode == 1)
    {
        motor_set_pwm(motor_id, 100);
    }
}

void _os_set_point_matrix(PikaObj *self, int point, int state)
{
    DrvLed_SetPointState((uint8_t)point, (uint8_t)state);
}
