#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// 显示模式
typedef enum {
    DISPLAY_MODE_STATIC = 0,     // 静态显示
    DISPLAY_MODE_HORIZONTAL,     // 横屏滚动
    DISPLAY_MODE_VERTICAL,       // 竖屏滚动
    DISPLAY_MODE_ANIMATION,      // 动画模式
    DISPLAY_MODE_OFF             // 关闭显示
} DisplayMode;

// 对齐方式
typedef enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT,
    ALIGN_TOP,
    ALIGN_BOTTOM
} Alignment;

// 显示配置
typedef struct {
    DisplayMode mode;
    uint8_t brightness;
    uint16_t scroll_speed_ms;    // 滚动速度（毫秒/像素）
    bool auto_clear;             // 自动清屏
    bool invert_display;         // 反色显示
} DisplayConfig;

// 初始化显示系统
void display_init(void);

// 配置显示
void display_set_config(DisplayConfig *config);
void display_get_config(DisplayConfig *config);

// 基本显示控制
void display_clear(void);
void display_on(void);
void display_off(void);
void display_set_brightness(uint8_t brightness);

// 像素操作
void display_load_pixel(uint8_t x, uint8_t y, bool state);
void display_set_pixel(uint8_t x, uint8_t y, bool state);
void display_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool state);
void display_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool state, bool fill);
void display_draw_circle(uint8_t x, uint8_t y, uint8_t radius, bool state);

// 文本显示
void display_text(const char *text, uint8_t x, uint8_t y, Alignment h_align, Alignment v_align);
void display_text_centered(const char *text);
void display_text_scrolling(const char *text, DisplayMode scroll_direction, uint16_t speed_ms);

// 滚动控制
void display_start_horizontal_scroll(bool direction_left, uint16_t speed_ms);
void display_start_vertical_scroll(bool direction_up, uint16_t speed_ms);
void display_stop_scroll(void);

// 动画控制
void display_play_power_on_animation(void);
void display_play_power_off_animation(void);
void display_play_ui_transition(bool direction_left);

// 缓冲区操作
void display_get_buffer(uint8_t *buffer, uint16_t size);
void display_set_buffer(uint8_t *buffer, uint16_t size);
void display_update(void);  // 手动刷新显示

// 测试功能
void display_test_pattern(void);
void display_test_text(void);


void matrix_callback(void *arg);
#endif /* __DISPLAY_H */
