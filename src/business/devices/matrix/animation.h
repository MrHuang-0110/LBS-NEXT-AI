#ifndef __ANIMATION_H
#define __ANIMATION_H
#include <stdint.h>
#include <stdbool.h>
#include "tm1640_config.h"

// 动画类型
typedef enum {
    ANIMATION_NONE = 0,
    ANIMATION_POWER_ON,          // 开机动画
    ANIMATION_POWER_OFF,         // 关机动画
    ANIMATION_HORIZONTAL_SCROLL, // 横屏滚动
    ANIMATION_VERTICAL_SCROLL,   // 竖屏滚动
    ANIMATION_UI_TRANSITION,     // UI切换（滑动）
    ANIMATION_UI_ZOOM,           // UI缩放动画
    ANIMATION_CUSTOM             // 自定义动画
} AnimationType;

// 动画状态
typedef enum {
    ANIMATION_STOPPED = 0,
    ANIMATION_RUNNING,
    ANIMATION_PAUSED,
    ANIMATION_COMPLETED
} AnimationState;

// 动画方向
typedef enum {
    DIRECTION_LEFT = 0,
    DIRECTION_RIGHT,
    DIRECTION_UP,
    DIRECTION_DOWN
} AnimationDirection;

// 蜂鸣器回调函数类型
typedef void (*BuzzerCallback)(void);

// 动画控制结构
typedef struct {
    AnimationType type;
    AnimationState state;
    AnimationDirection direction;
    uint32_t start_time;
    uint32_t duration_ms;
    uint32_t frame_interval_ms;
    uint8_t current_frame;
    uint8_t total_frames;
    bool repeat;
    uint8_t repeat_count;
    uint8_t repeat_counter;
    void *user_data;
} AnimationControl;

// 初始化动画系统
void animation_init(void);

// 注册蜂鸣器回调
void animation_register_buzzer_callback(BuzzerCallback power_on_cb, BuzzerCallback power_off_cb);

// 动画控制
void animation_start(AnimationType type, AnimationDirection direction, uint32_t duration_ms);
void animation_stop(void);
void animation_pause(void);
void animation_resume(void);
bool animation_is_running(void);
AnimationState animation_get_state(void);

// 特定动画控制
void animation_power_on(void);
void animation_power_off(void);
void animation_horizontal_scroll(AnimationDirection direction, uint32_t speed_ms);
void animation_vertical_scroll(AnimationDirection direction, uint32_t speed_ms);
void animation_ui_transition(AnimationDirection direction, uint32_t duration_ms);
void animation_ui_zoom(AnimationDirection direction, uint32_t duration_ms);
void animation_set_current_frame_buffer(uint8_t buffer[MATRIX_ROWS][MATRIX_COLS]);
void animation_set_next_frame_buffer(uint8_t buffer[MATRIX_ROWS][MATRIX_COLS]);

// 动画帧更新（需要在主循环中定期调用）
void animation_update(void);

// 自定义动画支持
void animation_set_custom_frames(uint8_t *frame_data, uint8_t frame_count, uint8_t frame_width, uint8_t frame_height);
void animation_start_custom(AnimationDirection direction, uint32_t duration_ms, bool repeat);

#endif /* __ANIMATION_H */
