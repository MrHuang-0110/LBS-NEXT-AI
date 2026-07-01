#include "led_matrix.h"
#include "tm1640.h"
#include "tm1640_config.h"
#include <string.h>
#include <stdlib.h>
// 帧缓冲区
static FrameBuffer frame_buffer = {0};

// 行到SEG的映射（行对应SEG阳极）
static const uint8_t row_to_seg[MATRIX_ROWS] = {
    SEG_ROW_1, SEG_ROW_2, SEG_ROW_3, SEG_ROW_4,
    SEG_ROW_5, SEG_ROW_6, SEG_ROW_7
};

// 列到GRID的映射（列对应GRID阴极）
static const uint8_t col_to_grid[MATRIX_COLS] = {
    GRID_COL_1, GRID_COL_2, GRID_COL_3, GRID_COL_4, GRID_COL_5
};

// TM1640显示缓冲区（16个字节，每个GRID一个字节）
static uint8_t tm1640_display_buffer[16] = {0};

// 初始化矩阵
void led_matrix_init(void) {
    // 初始化TM1640
TM1640_Config config = {
      .clk_port = GPIOB,
      .clk_pin = GPIO_PIN_6,
      .din_port = GPIOB,
      .din_pin = GPIO_PIN_7,
      .brightness = 7
  };
  tm1640_init(&config);

    // 清空缓冲区
    led_matrix_clear_all();
    led_matrix_refresh();
}

// 设置像素
void led_matrix_set_pixel(uint8_t row, uint8_t col, bool state) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return;
    frame_buffer[row][col] = state ? 1 : 0;
}

// 获取像素状态
bool led_matrix_get_pixel(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return false;
    return frame_buffer[row][col] != 0;
}

// 清除像素
void led_matrix_clear_pixel(uint8_t row, uint8_t col) {
    led_matrix_set_pixel(row, col, false);
}

// 切换像素
void led_matrix_toggle_pixel(uint8_t row, uint8_t col) {
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return;
    frame_buffer[row][col] = !frame_buffer[row][col];
}

// 清除所有像素
void led_matrix_clear_all(void) {
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

// 填充所有像素
void led_matrix_fill_all(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            frame_buffer[r][c] = 1;
        }
    }
}

// 设置整个缓冲区
void led_matrix_set_buffer(FrameBuffer *buffer) {
    if (buffer == NULL) return;
    memcpy(frame_buffer, buffer, sizeof(frame_buffer));
}

// 获取整个缓冲区
void led_matrix_get_buffer(FrameBuffer *buffer) {
    if (buffer == NULL) return;
    memcpy(buffer, frame_buffer, sizeof(frame_buffer));
}

// 复制缓冲区
void led_matrix_copy_buffer(FrameBuffer *src, FrameBuffer *dst) {
    if (src == NULL || dst == NULL) return;
    memcpy(dst, src, sizeof(FrameBuffer));
}

// 更新TM1640显示缓冲区（根据帧缓冲区）
static void update_tm1640_buffer(void) {
    // 清空TM1640缓冲区
    memset(tm1640_display_buffer, 0, sizeof(tm1640_display_buffer));

    // 遍历所有列（GRID阴极）
    for (uint8_t c = 0; c < MATRIX_COLS; c++) {
        uint8_t grid = col_to_grid[c];
        uint8_t grid_byte = 0;

        // 遍历所有行（SEG阳极），设置对应的位
        for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
            if (frame_buffer[r][c]) {
                uint8_t seg_bit = row_to_seg[r];
                if (seg_bit < 8) {
                    grid_byte |= (1 << seg_bit);
                } else {
                    // 如果SEG位大于7，需要特殊处理（TM1640的SEG8-15在第二个字节？）
                    // 暂时忽略，假设SEG0-7
                }
            }
        }

        tm1640_display_buffer[grid] = grid_byte;
    }
}

void led_cache_refresh(void)
{ 
   update_tm1640_buffer();
}
// 刷新显示
void led_matrix_refresh(void) {
    update_tm1640_buffer();
    // 发送整个TM1640缓冲区（16字节）
    tm1640_send_multiple_data(0, tm1640_display_buffer, 16);
}

// 部分刷新（从start_row到end_row）
void led_matrix_refresh_partial(uint8_t start_row, uint8_t end_row) {
    if (start_row >= MATRIX_ROWS || end_row >= MATRIX_ROWS || start_row > end_row) return;

    update_tm1640_buffer();

    // 只发送受影响的行
    for (uint8_t r = start_row; r <= end_row; r++) {
        // 注意：这里需要重新计算TM1640缓冲区，因为update_tm1640_buffer基于列
        // 对于部分刷新，更简单的方法是发送整个缓冲区
        // 暂时不支持部分刷新，回退到全刷新
    }
    // 部分刷新暂不支持，刷新整个显示
    led_matrix_refresh();
}

// 绘制直线（Bresenham算法）
void led_matrix_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool state) {
    // 确保坐标在矩阵范围内
    if (x1 >= MATRIX_COLS || x2 >= MATRIX_COLS || y1 >= MATRIX_ROWS || y2 >= MATRIX_ROWS) return;

    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t e2;

    while (1) {
        led_matrix_set_pixel(y1, x1, state);

        if (x1 == x2 && y1 == y2) break;

        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// 绘制矩形
void led_matrix_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool state, bool fill) {
    if (x >= MATRIX_COLS || y >= MATRIX_ROWS) return;

    uint8_t x_end = x + width - 1;
    uint8_t y_end = y + height - 1;
    if (x_end >= MATRIX_COLS) x_end = MATRIX_COLS - 1;
    if (y_end >= MATRIX_ROWS) y_end = MATRIX_ROWS - 1;

    if (fill) {
        for (uint8_t r = y; r <= y_end; r++) {
            for (uint8_t c = x; c <= x_end; c++) {
                led_matrix_set_pixel(r, c, state);
            }
        }
    } else {
        // 绘制四条边
        for (uint8_t c = x; c <= x_end; c++) {
            led_matrix_set_pixel(y, c, state);
            led_matrix_set_pixel(y_end, c, state);
        }
        for (uint8_t r = y; r <= y_end; r++) {
            led_matrix_set_pixel(r, x, state);
            led_matrix_set_pixel(r, x_end, state);
        }
    }
}

// 绘制圆（中点圆算法）
void led_matrix_draw_circle(uint8_t x0, uint8_t y0, uint8_t radius, bool state) {
    if (x0 >= MATRIX_COLS || y0 >= MATRIX_ROWS) return;

    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // 绘制八个对称点
        led_matrix_set_pixel(y0 + y, x0 + x, state);
        led_matrix_set_pixel(y0 + x, x0 + y, state);
        led_matrix_set_pixel(y0 + x, x0 - y, state);
        led_matrix_set_pixel(y0 + y, x0 - x, state);
        led_matrix_set_pixel(y0 - y, x0 - x, state);
        led_matrix_set_pixel(y0 - x, x0 - y, state);
        led_matrix_set_pixel(y0 - x, x0 + y, state);
        led_matrix_set_pixel(y0 - y, x0 + x, state);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// 测试模式
void led_matrix_test_pattern(void) {
    led_matrix_clear_all();

    // 绘制边框
    led_matrix_draw_rect(0, 0, MATRIX_COLS, MATRIX_ROWS, true, false);

    // 绘制对角线
    led_matrix_draw_line(0, 0, MATRIX_COLS - 1, MATRIX_ROWS - 1, true);
    led_matrix_draw_line(MATRIX_COLS - 1, 0, 0, MATRIX_ROWS - 1, true);

    // 绘制中心圆
    uint8_t center_x = MATRIX_COLS / 2;
    uint8_t center_y = MATRIX_ROWS / 2;
    uint8_t radius = (center_x < center_y) ? center_x : center_y;
    if (radius > 0) radius -= 1;
    led_matrix_draw_circle(center_x, center_y, radius, true);

    led_matrix_refresh();
} 
