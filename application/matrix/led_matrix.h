#ifndef __LED_MATRIX_H
#define __LED_MATRIX_H

#include <stdint.h>
#include <stdbool.h>

// 矩阵尺寸（在tm1640_config.h中定义）
#include "tm1640_config.h"

// 帧缓冲区类型
typedef uint8_t FrameBuffer[MATRIX_ROWS][MATRIX_COLS];

// 矩阵初始化
void led_matrix_init(void);

// 基本像素操作
void led_matrix_set_pixel(uint8_t row, uint8_t col, bool state);
bool led_matrix_get_pixel(uint8_t row, uint8_t col);
void led_matrix_clear_pixel(uint8_t row, uint8_t col);
void led_matrix_toggle_pixel(uint8_t row, uint8_t col);

// 缓冲区操作
void led_matrix_clear_all(void);
void led_matrix_fill_all(void);
void led_matrix_set_buffer(FrameBuffer *buffer);
void led_matrix_get_buffer(FrameBuffer *buffer);
void led_matrix_copy_buffer(FrameBuffer *src, FrameBuffer *dst);

// 图形操作
void led_matrix_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool state);
void led_matrix_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool state, bool fill);
void led_matrix_draw_circle(uint8_t x0, uint8_t y0, uint8_t radius, bool state);

// 显示刷新
void led_matrix_refresh(void);
void led_cache_refresh(void);
void led_matrix_refresh_partial(uint8_t start_row, uint8_t end_row);

// 测试模式
void led_matrix_test_pattern(void);

#endif /* __LED_MATRIX_H */
