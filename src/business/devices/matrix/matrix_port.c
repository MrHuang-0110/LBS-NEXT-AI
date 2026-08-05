#include "matrix_port.h"
#include "led_matrix.h"
#include "display.h"
#include "font.h"
#include "tm1640.h"
#include "tm1640_config.h"
#include <string.h>
#include "stm32f1xx_hal.h"
#include "PikaVM.h"
static bool initialized = false;

// 内部函数声明
static void render_text_at_offset(const char *str, uint16_t offset);
static uint16_t calculate_text_width(const char *str);
static void delay_ms(uint32_t ms);
static bool delay_ms_with_check(uint32_t ms, matrix_port_exit_check_t exit_check);

/**
 * @brief 简单延时函数
 */
static void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

/**
 * @brief 带退出检查的延时函数
 * @return true: 因退出条件提前返回，false: 正常完成延时
 */
static bool delay_ms_with_check(uint32_t ms, matrix_port_exit_check_t exit_check) {
    if (exit_check == NULL) {
        // 没有退出检查，使用普通延时
        HAL_Delay(ms);
        return false;
    }

    // 将长延时分解为多个短延时，每次检查退出条件
    const uint32_t CHECK_INTERVAL = 10; // 10ms检查一次
    uint32_t remaining = ms;

    while (remaining > 0) {
        uint32_t delay_time = (remaining > CHECK_INTERVAL) ? CHECK_INTERVAL : remaining;
        HAL_Delay(delay_time);
        remaining -= delay_time;

        // 检查退出条件
        if (exit_check()) {
            return true; // 因退出条件提前返回
        }
    }

    return false; // 正常完成延时
}

/**
 * @brief 计算文本像素宽度
 */
static uint16_t calculate_text_width(const char *str) {
    if (str == NULL) return 0;

    uint16_t len = strlen(str);
    if (len == 0) return 0;

    // 每个字符5像素，字符间1像素空格
    return len * 5 + (len - 1);
}

/**
 * @brief 在指定偏移位置渲染文本
 */
static void render_text_at_offset(const char *str, uint16_t offset) {
    if (str == NULL) return;

    // 清空矩阵
    led_matrix_clear_all();

    uint16_t current_pixel = offset;
    uint16_t char_index = 0;
    uint8_t char_pixel_offset = 0;

    // 查找当前偏移对应的字符和列
    while (char_index < strlen(str) && current_pixel >= 5) {
        current_pixel -= 5;
        char_index++;
        if (char_index < strlen(str)) {
            // 跳过字符间空格（1像素）
            if (current_pixel > 0) {
                current_pixel--;
            }
        }
    }

    if (char_index < strlen(str)) {
        char_pixel_offset = current_pixel;

        // 渲染从char_index开始的字符，直到填满5列
        uint8_t col = 0;
        while (col < MATRIX_PORT_COLS && char_index < strlen(str)) {
            uint8_t ch = str[char_index];
            if (ch < 32 || ch > 127) ch = ' ';

            const uint8_t *font_data = font_get_char_data(ch);
            if (font_data == NULL) {
                char_index++;
                char_pixel_offset = 0;
                continue;
            }

            // 渲染该字符的剩余列
            for (uint8_t font_col = char_pixel_offset; font_col < 5 && col < MATRIX_PORT_COLS; font_col++) {
                uint8_t col_data = font_data[font_col];
                for (uint8_t row = 0; row < 7; row++) {
                    if (col_data & (1 << row)) {
                        led_matrix_set_pixel(row, col, true);
                    }
                }
                col++;
            }

            char_index++;
            char_pixel_offset = 0;

            // 字符间空格（如果还有下一个字符）
            if (char_index < strlen(str) && col < MATRIX_PORT_COLS) {
                col++; // 空格列
            }
        }
    }

    // 刷新显示
    led_matrix_refresh();
}

bool uvm_exit(void)
{
  return (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT);
}
/**
 * @brief 初始化矩阵端口
 */
void matrix_port_init(void) {
    if (initialized) return;

    // 初始化LED矩阵（内部会初始化TM1640）
    led_matrix_init();

    initialized = true;
}

/**
 * @brief 显示图案
 */
void matrix_port_display_pattern(const uint8_t pattern[7]) {
    if (!initialized) matrix_port_init();

    // 清空矩阵
    led_matrix_clear_all();

    // 解析图案数据：每行一个字节，bit0对应第0列（最左列），bit4对应第4列（最右列）
    // 注意：x代表行(0-6)，y代表列(0-4)
    for (uint8_t row = 0; row < MATRIX_PORT_ROWS; row++) {
        uint8_t row_data = pattern[row];
        // 只取低5位
        row_data &= 0x1F;

        for (uint8_t col = 0; col < MATRIX_PORT_COLS; col++) {
            // 检查对应位是否置位
            if (row_data & (1 << col)) {
                // row是行，col是列
                led_matrix_set_pixel(row, col, true);
            }
        }
    }

    // 立即刷新显示
    led_matrix_refresh();
}

/**
 * @brief 滚动显示字符串（阻塞式）
 */
void matrix_port_scroll_text(const char *str, matrix_port_exit_check_t exit_check) {
    if (!initialized) matrix_port_init();

    if (str == NULL) return;

    uint16_t len = strlen(str);
    if (len == 0) return;

    // 计算文本总宽度
    uint16_t text_width = calculate_text_width(str);

    if (len == 1 || text_width <= MATRIX_PORT_COLS) {
        // 单字符或文本宽度小于等于显示宽度，居中显示
        // 清屏
        led_matrix_clear_all();

        // 检查退出条件（在显示前检查）
        if (exit_check != NULL && exit_check()) {
            // 退出显示
            return;
        }

        // 计算居中位置
        uint8_t start_col = (MATRIX_PORT_COLS - text_width) / 2;

        // 渲染文本
        uint8_t current_col = start_col;
        for (uint16_t i = 0; i < len && current_col < MATRIX_PORT_COLS; i++) {
            uint8_t ch = str[i];
            if (ch < 32 || ch > 127) ch = ' ';

            const uint8_t *font_data = font_get_char_data(ch);
            if (font_data == NULL) continue;

            // 渲染字符（最多5列）
            for (uint8_t font_col = 0; font_col < 5 && current_col < MATRIX_PORT_COLS; font_col++) {
                uint8_t col_data = font_data[font_col];
                for (uint8_t row = 0; row < 7; row++) {
                    if (col_data & (1 << row)) {
                        led_matrix_set_pixel(row, current_col, true);
                    }
                }
                current_col++;
            }

            // 字符间空格（如果还有下一个字符且还有空间）
            if (i < len - 1 && current_col < MATRIX_PORT_COLS) {
                current_col++; // 空格列
            }
        }

        led_matrix_refresh();
        return;
    }

    // 多字符滚动显示
    // 计算需要滚动的总帧数
    uint16_t total_frames = text_width - MATRIX_PORT_COLS;

    // 循环滚动（文本从右侧进入，向左移动）
    for (uint16_t offset = 0; offset <= total_frames; offset++) {
        // 检查退出条件
        if (exit_check != NULL && exit_check()) {
            // 退出滚动，清屏
            led_matrix_clear_all();
            led_matrix_refresh();
            return;
        }

        render_text_at_offset(str, offset);
        // 延时并检查退出条件
        if (delay_ms_with_check(100, exit_check)) {
            // 延时期间检测到退出条件，清屏并返回
            led_matrix_clear_all();
            led_matrix_refresh();
            return;
        }
    }

    // 滚动完成后显示文本的最后部分（不清屏）
    // 最后偏移量显示文本末尾
}

/**
 * @brief 设置矩阵灯整体亮度
 */
void matrix_port_set_brightness(uint8_t brightness) {
    if (!initialized) matrix_port_init();

    if (brightness > 7) brightness = 7;

    // 调用TM1640亮度设置
    tm1640_set_brightness(brightness);
}

/**
 * @brief 点亮指定LED灯
 */
void matrix_port_set_pixel(uint8_t x, uint8_t y) {
    if (!initialized) matrix_port_init();

    // x是行(0-6)，y是列(0-4)
    if (x >= MATRIX_PORT_ROWS || y >= MATRIX_PORT_COLS) return;

    // 设置像素（x行，y列）
    led_matrix_set_pixel(x, y, true);

    // 立即刷新显示
    led_matrix_refresh();
}

/**
 * @brief 设置指定LED灯的亮度（实际为开关）
 */
void matrix_port_set_pixel_brightness(uint8_t x, uint8_t y, uint8_t b) {
    if (!initialized) matrix_port_init();

    // x是行(0-6)，y是列(0-4)
    if (x >= MATRIX_PORT_ROWS || y >= MATRIX_PORT_COLS) return;

    // TM1640不支持单个LED亮度控制，只能开关
    // b=0熄灭，b>0点亮
    led_matrix_set_pixel(x, y, (b > 0));

    // 立即刷新显示
    led_matrix_refresh();
}

/**
 * @brief 清屏
 */
void matrix_port_clear(void) {
    if (!initialized) matrix_port_init();

    // 清空矩阵并刷新
    led_matrix_clear_all();
    led_matrix_refresh();
}