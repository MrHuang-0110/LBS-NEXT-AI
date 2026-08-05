
#include "key.h"
#include "delay.h"
#include "event_manager.h"

volatile bool start_py = false;
volatile bool start_pauto = false;
static REMOTE_CFG remote_cfg;

/* Task3: 长短按/按住进度/释放回调（由应用层注册，driver 层不依赖业务头文件） */
static void (*s_short_press_cb)(void) = NULL;
static void (*s_long_press_cb)(void) = NULL;
static void (*s_hold_progress_cb)(uint8_t lit) = NULL;
static void (*s_release_cb)(void) = NULL;

void Key_RegisterShortPressCb(void (*cb)(void)) { s_short_press_cb = cb; }
void Key_RegisterLongPressCb(void (*cb)(void))  { s_long_press_cb = cb; }
void Key_RegisterHoldProgressCb(void (*cb)(uint8_t lit)) { s_hold_progress_cb = cb; }
void Key_RegisterReleaseCb(void (*cb)(void))    { s_release_cb = cb; }

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

#define KEY_CLICK_MIN_MS        30U
#define KEY_CLICK_MAX_MS        800U
#define KEY_HOLD_SHUTDOWN_MS    1500U
#define KEY_BOOT_IGNORE_MS      800U

static uint32_t s_key_ready_tick;

/* 按键判定状态机：由 key_middle_event 在 TIM6 事件回调（ISR 上下文）中驱动，
 * 脚本运行期间主循环阻塞时按键仍可用。注意：回调只做状态判定与事件上抛，
 * 禁止在回调内执行阻塞/耗时操作（关机序列通过 g_shutdown_pending 挂起消费）。 */
void Key_Scan_Handler(uint32_t scan_interval_ms)
{
    (void)scan_interval_ms;
    uint32_t now = HAL_GetTick();

    if ((now - s_key_ready_tick) < KEY_BOOT_IGNORE_MS) { return; }   /* 开机忽略窗口（起始 tick 由 Key_EnableAfterBoot 记录，减法防回绕） */
    key_handle.current_raw_state = key_read_raw_pressed();

    if (key_handle.current_raw_state == 1U)   /* 按下 */
    {
        if (key_handle.press_start_time == 0U)
        {
            key_handle.press_start_time = now;
        }
        if (s_hold_progress_cb != NULL)
        {
            uint32_t held = now - key_handle.press_start_time;
            uint8_t group = (uint8_t)((held * 3U) / KEY_HOLD_SHUTDOWN_MS);
            if (group > 3U) { group = 3U; }
            s_hold_progress_cb((uint8_t)(group * 3U));
        }
        if ((now - key_handle.press_start_time) >= KEY_HOLD_SHUTDOWN_MS)
        {
            if (s_long_press_cb != NULL) { s_long_press_cb(); }
            key_handle.press_start_time = 0U;   /* 防止重复触发 */
        }
    }
    else if (key_handle.press_start_time != 0U) /* 释放 */
    {
        uint32_t held = now - key_handle.press_start_time;
        key_handle.press_start_time = 0U;
        if ((held > KEY_CLICK_MIN_MS) && (held < KEY_CLICK_MAX_MS))
        {
            if (s_short_press_cb != NULL) { s_short_press_cb(); }
        }
        else if (held >= KEY_CLICK_MAX_MS)  /* 非短按释放（长按中止/超窗释放） */
        {
            if (s_release_cb != NULL) { s_release_cb(); }
        }
    }
    key_handle.last_raw_state = key_handle.current_raw_state;
}

void Key_EnableAfterBoot(void)
{
    s_key_ready_tick = HAL_GetTick();
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
