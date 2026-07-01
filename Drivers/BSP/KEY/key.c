
#include "./BSP/KEY/key.h"
#include "./SYSTEM/delay/delay.h"
#include "pikaVM.h"
#include "event_manager.h"
#include "beep.h"
#include "app_pika_runtime.h"
#include "lbsfilemanager.h"

volatile bool start_py = false;
volatile bool start_pauto = false;
static REMOTE_CFG remote_cfg;

static Key_Scan_Handle_t key_handle = {
    .port = KEY1_GPIO_PORT,
    .pin = KEY1_GPIO_PIN,
    .active_level = GPIO_PIN_RESET,
    .state = KEY_STATE_IDLE,
    .short_press_flag = 0,
    .long_press_flag = 0,
    .long_hold_flag = 0,
    .press_start_time = 0,
    .press_duration = 0,
    .last_long_hold_time = 0,
    .debounce_time = 10,
    .long_press_time = 1500,
    .long_hold_interval = 500,
    .current_raw_state = 0,
    .last_raw_state = 0
};

void set_entery_short(uint8_t key)
{
    key_handle.short_press_flag = key;
}

static uint8_t key_read_raw_pressed(void)
{
    return (HAL_GPIO_ReadPin(key_handle.port, key_handle.pin) == key_handle.active_level) ? 1U : 0U;
}

void Key_Scan_Handler(uint32_t scan_interval_ms)
{
    (void)scan_interval_ms;
    key_handle.current_raw_state = key_read_raw_pressed();

    uint8_t rising_edge = (key_handle.current_raw_state == 0 && key_handle.last_raw_state == 1);
    key_handle.last_raw_state = key_handle.current_raw_state;

    if (rising_edge && start_py)
    {
        __exitpython();
        start_pauto = false;
    }
}

uint8_t Key_Check_Short_Press(void)
{
    if (key_handle.short_press_flag)
    {
        key_handle.short_press_flag = 0;
        return 1;
    }
    return 0;
}

uint8_t Key_Check_Long_Press(void)
{
    if (key_handle.long_press_flag)
    {
        key_handle.long_press_flag = 0;
        return 1;
    }
    return 0;
}

uint8_t Key_Check_Long_Hold(void)
{
    if (key_handle.long_hold_flag)
    {
        key_handle.long_hold_flag = 0;
        return 1;
    }
    return 0;
}

uint32_t Key_Get_Press_Duration(void)
{
    if (key_handle.current_raw_state == 1)
    {
        return HAL_GetTick() - key_handle.press_start_time;
    }
    return 0;
}

uint8_t Key_Is_Pressed(void)
{
    return key_read_raw_pressed();
}

void Key_Reset_State(void)
{
    key_handle.state = KEY_STATE_IDLE;
    key_handle.short_press_flag = 0;
    key_handle.long_press_flag = 0;
    key_handle.long_hold_flag = 0;
    key_handle.last_long_hold_time = 0;
}

void Key_Config_Params(uint16_t debounce_ms, uint16_t long_press_ms, uint16_t hold_interval_ms)
{
    key_handle.debounce_time = debounce_ms;
    key_handle.long_press_time = long_press_ms;
    key_handle.long_hold_interval = hold_interval_ms;
}

void key_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    KEY1_GPIO_CLK_ENABLE();
    gpio_init_struct.Pin = KEY1_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY1_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = BLUE_STA_GPIO_PIN;
    gpio_init_struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BLUE_STA_PORT, &gpio_init_struct);

    key_handle.current_raw_state = key_read_raw_pressed();
    key_handle.last_raw_state = key_handle.current_raw_state;
}

int read_advance_offset1(void) { return remote_cfg.advance_offset1; }
int read_advance_offset2(void) { return remote_cfg.advance_offset2; }
int read_retreat_offset1(void) { return remote_cfg.retreat_offset1; }
int read_retreat_offset2(void) { return remote_cfg.retreat_offset2; }

void write_advance_remote_cfg(int offset1, int offset2)
{
    remote_cfg.advance_offset1 = offset1;
    remote_cfg.advance_offset2 = offset2;
}

void write_retreat_remote_cfg(int offset1, int offset2)
{
    remote_cfg.retreat_offset1 = offset1;
    remote_cfg.retreat_offset2 = offset2;
}

void loader_remote_cfg(void)
{
}

void key_middle_callback(void *arg)
{
    Key_Scan_Handler(10);
}
