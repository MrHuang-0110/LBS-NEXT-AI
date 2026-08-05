#include "adc.h"
#include "led.h"
#include "drv_led.h"
#include "bat_manager.h"

#include "./SYSTEM/delay/delay.h"

extern void usb_printf(char* fmt,...);
static BatteryManager battery_mgr = {0};

int calculate_battery_percentage(float voltage, float min_v, float max_v)
{
    if (voltage <= min_v)
    {
        return 0;
    }
    if (voltage >= max_v)
    {
        return 100;
    }
    return (int)((voltage - min_v) / (max_v - min_v) * 100.0f);
}

static BatteryLevel get_battery_level(float voltage)
{
    int pct;

    if (voltage < BAT_VOLTAGE_EMPTY)
    {
        return BATTERY_CRITICAL;
    }
    pct = calculate_battery_percentage(voltage, BAT_VOLTAGE_EMPTY, BAT_VOLTAGE_FULL);
    if (pct < BAT_LOW_PERCENT)
    {
        return BATTERY_LOW;
    }
    if (pct < 60)
    {
        return BATTERY_MID;
    }
    return BATTERY_HIGH;
}

/*
 * ????????????????
 * PB0 = LED0 = ???PB1 = LED1 = ?????????
 */
static void set_battery_leds(BatteryLevel level)
{
    switch (level)
    {
        case BATTERY_CRITICAL:
        case BATTERY_LOW:
            LED0(0);  /* ??? */
            LED1(1);  /* ??? */
            break;

        case BATTERY_MID:
        case BATTERY_HIGH:
            LED1(0);  /* ??? */
            LED0(1);  /* ??? */
            break;

        case BATTERY_INVALID:
        default:
            break;
    }
}

void battery_manager_init(void)
{
    battery_mgr.buffer_index = 0;
    battery_mgr.filtered_voltage = 0.0f;
    battery_mgr.last_level = BATTERY_INVALID;
    battery_mgr.last_check_time = 0U;
    battery_mgr.low_battery_count = 0;
#if WIALL_HARDWARE_ENABLE
    LED0(1);
    LED1(1);
#endif
}

void update_battery_voltage(float new_voltage)
{
    battery_mgr.voltage_buffer[battery_mgr.buffer_index] = new_voltage;
    battery_mgr.buffer_index = (battery_mgr.buffer_index + 1) % 10;

    float sum = 0;
    for (int i = 0; i < 10; i++)
    {
        sum += battery_mgr.voltage_buffer[i];
    }
    battery_mgr.filtered_voltage = sum / 10.0f;
}

float get_bat_filtered_volatge(void)
{
    return battery_mgr.filtered_voltage;
}

void check_battery_with_debounce(void)
{
    static uint32_t last_update_time = 0;
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_update_time < 1000U)
    {
        return;
    }
    last_update_time = current_time;

    BatteryLevel new_level = get_battery_level(battery_mgr.filtered_voltage);

    if (new_level == BATTERY_CRITICAL)
    {
        battery_mgr.low_battery_count++;
        if (battery_mgr.low_battery_count >= 3U)
        {
            if (battery_mgr.last_level != BATTERY_CRITICAL)
            {
                set_battery_leds(BATTERY_CRITICAL);
                battery_mgr.last_level = BATTERY_CRITICAL;
            }
        }
    }
    else
    {
        battery_mgr.low_battery_count = 0;
        if (new_level != battery_mgr.last_level)
        {
            set_battery_leds(new_level);
            battery_mgr.last_level = new_level;
        }
    }
}

void battery_check_callback(void *arg)
{
    (void)arg;
    check_battery_with_debounce();
}
