/**
 * test_shutdown_flow.c — 关机动画被按键事件干扰 单元测试（host 模拟）
 *
 * 编译: gcc -Wall -Wextra -std=c99 -o test_shutdown_flow test_shutdown_flow.c && ./test_shutdown_flow
 *
 * 背景: 重构后按键状态机移入 TIM6 ISR 的 key_middle_event（1ms 一次）。
 *       长按触发关机（g_shutdown_pending）后，app_shutdown_sequence() 阻塞播放
 *       关机动画（HAL_Delay 期间 TIM6 ISR 照跑）。用户手指未松开时，
 *       Key_Scan_Handler 的 press_start_time 被清零后重新计时，hold_progress 回调
 *       持续上报 lit=0→3→6→9 循环，反复改写流水灯 GPIO，与关机动画抢控制权；
 *       松手还会触发 release 回调重新打开流水灯。修复：关机序列开始时
 *       set_event_disable("key_middle_event")。
 *
 * 本测试复制生产代码（src/driver/bsp/key.c 的 Key_Scan_Handler 与
 * src/middleware/event_manager.c 的调度逻辑）到 host，用 mock tick/GPIO 模拟时序。
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* Mock 环境                                                          */
/* ------------------------------------------------------------------ */

static uint32_t g_mock_tick;          /* 模拟 HAL_GetTick() */
static uint8_t  g_mock_key_pressed;   /* 模拟按键 GPIO 电平（1=按下） */

#define KEY_HOLD_SHUTDOWN_MS    1500U   /* 与 key.c 一致 */
#define KEY_BOOT_IGNORE_MS      800U
#define KEY_CLICK_MIN_MS        30U
#define KEY_CLICK_MAX_MS        800U

static uint32_t mock_tick(void)          { return g_mock_tick; }
static uint8_t  mock_key_pressed(void)   { return g_mock_key_pressed; }

/* ------------------------------------------------------------------ */
/* 复制 Key_Scan_Handler 状态机（src/driver/bsp/key.c）                */
/* 仅替换 HAL_GetTick() → mock_tick()、key_read_raw_pressed() → mock   */
/* ------------------------------------------------------------------ */

static void (*s_short_press_cb)(void) = NULL;
static void (*s_long_press_cb)(void) = NULL;
static void (*s_hold_progress_cb)(uint8_t lit) = NULL;
static void (*s_release_cb)(void) = NULL;

static uint32_t s_key_ready_tick;

typedef struct {
    uint32_t press_start_time;
    uint8_t  current_raw_state;
    uint8_t  last_raw_state;
} Key_Scan_Handle_t;

static Key_Scan_Handle_t key_handle = {0};

void Key_RegisterShortPressCb(void (*cb)(void)) { s_short_press_cb = cb; }
void Key_RegisterLongPressCb(void (*cb)(void))  { s_long_press_cb = cb; }
void Key_RegisterHoldProgressCb(void (*cb)(uint8_t lit)) { s_hold_progress_cb = cb; }
void Key_RegisterReleaseCb(void (*cb)(void))    { s_release_cb = cb; }

void Key_EnableAfterBoot(void) { s_key_ready_tick = g_mock_tick; }

/* 与生产 Key_Scan_Handler 逐行一致（仅换 mock） */
void Key_Scan_Handler(uint32_t scan_interval_ms)
{
    (void)scan_interval_ms;
    uint32_t now = mock_tick();

    if ((now - s_key_ready_tick) < KEY_BOOT_IGNORE_MS) { return; }
    key_handle.current_raw_state = mock_key_pressed();

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

void key_middle_callback(void *arg)
{
    (void)arg;
    Key_Scan_Handler(10);
}

/* ------------------------------------------------------------------ */
/* 复制 event_manager 调度（src/middleware/event_manager.c 精简）      */
/* ------------------------------------------------------------------ */

#define EVENT_MAX 16

typedef struct {
    char name[20];   /* 与生产 Event_t 一致（修复后）：16 字符事件名 + '\0' 不会越界 */
    uint32_t threshold_ticks;
    uint32_t accum_ticks;
    void (*callback)(void *arg);
    void *arg;
    bool enabled;
} Event_t;

static Event_t g_events[EVENT_MAX];
static uint32_t g_event_count = 0;

static void event_register(const char *name, uint32_t threshold_ticks,
                           void (*callback)(void *), void *arg)
{
    if (g_event_count >= EVENT_MAX) return;
    memset(g_events[g_event_count].name, 0, sizeof(g_events[g_event_count].name));
    strncpy(g_events[g_event_count].name, name, sizeof(g_events[g_event_count].name) - 1U);   /* 与生产 strncpy 一致 */
    g_events[g_event_count].threshold_ticks = threshold_ticks;
    g_events[g_event_count].accum_ticks = 0;
    g_events[g_event_count].callback = callback;
    g_events[g_event_count].arg = arg;
    g_events[g_event_count].enabled = true;
    g_event_count++;
}

static int event_find(const char *name)
{
    uint32_t i;
    for (i = 0U; i < g_event_count; i++)
    {
        if (strcmp(g_events[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static uint32_t g_last_cpu_tick = 0;

static void event_schedlucer(uint32_t cpu_tick)
{
    uint32_t i;
    for (i = 0U; i < g_event_count; i++)
    {
        if (!g_events[i].enabled) continue;
        g_events[i].accum_ticks += cpu_tick - g_last_cpu_tick;
        if (g_events[i].accum_ticks >= g_events[i].threshold_ticks)
        {
            if (g_events[i].callback != NULL)
            {
                g_events[i].callback(g_events[i].arg);
            }
            g_events[i].accum_ticks = 0;
        }
    }
    g_last_cpu_tick = cpu_tick;
}

static void set_event_disable(const char *name)
{
    int id = event_find(name);
    if (id >= 0) g_events[id].enabled = false;
}

/* ------------------------------------------------------------------ */
/* 统计回调                                                           */
/* ------------------------------------------------------------------ */

static int g_progress_calls = 0;
static int g_long_press_calls = 0;
static int g_release_calls = 0;
static uint8_t g_last_lit = 0;

static void on_progress(uint8_t lit) { g_progress_calls++; g_last_lit = lit; }
static void on_long_press(void)      { g_long_press_calls++; }
static void on_release(void)         { g_release_calls++; }

static void reset_stats(void)
{
    g_progress_calls = 0;
    g_long_press_calls = 0;
    g_release_calls = 0;
    g_last_lit = 0;
}

/* 每个测试开头的公共初始化 */
static void setup_test(void)
{
    memset(g_events, 0, sizeof(g_events));
    g_event_count = 0;
    g_last_cpu_tick = 0;
    reset_stats();
    memset(&key_handle, 0, sizeof(key_handle));
    g_mock_tick = 0U;
    g_mock_key_pressed = 0U;
}

/* 推进 N ms：每 ms 调一次调度器（模拟 TIM6 1ms 事件调度） */
static void advance_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0U; i < ms; i++)
    {
        g_mock_tick++;
        event_schedlucer(g_mock_tick);
    }
}

/* ------------------------------------------------------------------ */
/* 测试用例                                                           */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { \
    printf("  %-58s ", name); \
} while (0)

#define PASS() do { \
    printf("PASS\n"); g_pass++; \
} while (0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); g_fail++; \
} while (0)

/* Test 1: 长按 1.5s 触发关机；触发后手指未松，进度回调继续上报（复现干扰） */
static void test_hold_after_long_press_keeps_reporting(void)
{
    TEST("bug: hold after long-press keeps progress reporting");

    setup_test();

    Key_RegisterShortPressCb(NULL);
    Key_RegisterLongPressCb(on_long_press);
    Key_RegisterHoldProgressCb(on_progress);
    Key_RegisterReleaseCb(on_release);
    Key_EnableAfterBoot();
    event_register("key_middle_event", 1, key_middle_callback, NULL);

    advance_ms(KEY_BOOT_IGNORE_MS);        /* 越过开机忽略窗口 */
    g_mock_key_pressed = 1U;               /* 按下并保持 */

    /* 长按 1.5s（+10ms 余量，press_start_time 在首次扫描才置位）：应触发一次 long_press */
    advance_ms(KEY_HOLD_SHUTDOWN_MS + 10U);
    if (g_long_press_calls != 1)
    {
        FAIL("expected exactly 1 long_press after 1.5s hold");
        return;
    }

    /* 触发后手指未松，继续按住 1.5s：
     * press_start_time 被清零 → 重新计时 → 进度回调继续上报（干扰关机动画） */
    advance_ms(KEY_HOLD_SHUTDOWN_MS);
    if (g_progress_calls <= 1)
    {
        FAIL("expected progress to keep reporting while still held (interference)");
        return;
    }
    PASS();
}

/* Test 2: 关机序列开始时禁用 key_middle_event → 按住期间不再上报/重触发 */
static void test_disable_key_event_stops_interference(void)
{
    TEST("fix: disable key_middle_event stops progress/long-press");

    setup_test();

    Key_RegisterShortPressCb(NULL);
    Key_RegisterLongPressCb(on_long_press);
    Key_RegisterHoldProgressCb(on_progress);
    Key_RegisterReleaseCb(on_release);
    Key_EnableAfterBoot();
    event_register("key_middle_event", 1, key_middle_callback, NULL);

    advance_ms(KEY_BOOT_IGNORE_MS);
    g_mock_key_pressed = 1U;

    advance_ms(KEY_HOLD_SHUTDOWN_MS + 10U);
    if (g_long_press_calls != 1)
    {
        FAIL("expected exactly 1 long_press before disable");
        return;
    }
    int progress_before = g_progress_calls;

    /* 模拟 app_shutdown_sequence() 开头禁用按键事件 */
    set_event_disable("key_middle_event");

    /* 继续按住 3s：进度/长按/释放回调均不应再触发（动画期间 LED 不被改写） */
    advance_ms(3000U);
    if (g_progress_calls != progress_before)
    {
        FAIL("progress should not be reported after event disabled");
        return;
    }
    if (g_long_press_calls != 1)
    {
        FAIL("long_press should not re-trigger after event disabled");
        return;
    }

    /* 松手：release 也不应触发（否则会 DrvLed_SetFlowEnable(1) 重开流水灯） */
    g_mock_key_pressed = 0U;
    advance_ms(50U);
    if (g_release_calls != 0)
    {
        FAIL("release should not fire after event disabled");
        return;
    }
    PASS();
}

/* Test 3: 未禁用时松手会触发 release（对照，说明禁用必要性） */
static void test_release_fires_without_disable(void)
{
    TEST("bug: release fires while holding shutdown (would reopen flow LED)");

    setup_test();

    Key_RegisterShortPressCb(NULL);
    Key_RegisterLongPressCb(on_long_press);
    Key_RegisterHoldProgressCb(on_progress);
    Key_RegisterReleaseCb(on_release);
    Key_EnableAfterBoot();
    event_register("key_middle_event", 1, key_middle_callback, NULL);

    advance_ms(KEY_BOOT_IGNORE_MS);
    g_mock_key_pressed = 1U;
    advance_ms(KEY_HOLD_SHUTDOWN_MS + 10U);   /* 触发长按（press_start_time 清零） */
    advance_ms(900U);                          /* 继续按住：重新计时且 held>=800，模拟动画期间未松手 */
    g_mock_key_pressed = 0U;            /* 关机动画期间松手 */
    advance_ms(50U);

    if (g_release_calls != 1)
    {
        FAIL("expected release to fire when key released without disable");
        return;
    }
    PASS();
}

/* Test 4: 16 字符事件名可被 find/disable（回归：name[16] 越界导致 strcmp 失败） */
static void test_16char_event_name_findable(void)
{
    TEST("regression: 16-char event name is findable & disableable");

    setup_test();

    /* "key_middle_event" 恰好 16 字符；name 数组必须能容纳 + '\0' */
    event_register("key_middle_event", 1, key_middle_callback, NULL);
    int id = event_find("key_middle_event");
    if (id < 0)
    {
        FAIL("16-char event name not findable (name buffer overflow?)");
        return;
    }
    set_event_disable("key_middle_event");
    if (g_events[id].enabled != false)
    {
        FAIL("event not disabled after set_event_disable");
        return;
    }
    PASS();
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== 关机动画被按键事件干扰 单元测试 ===\n\n");

    test_hold_after_long_press_keeps_reporting();
    test_disable_key_event_stops_interference();
    test_release_fires_without_disable();
    test_16char_event_name_findable();

    printf("\n=== 结果: %d PASS, %d FAIL ===\n\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
