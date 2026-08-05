#include "display.h"
#include "led_matrix.h"
#include "font.h"
#include "animation.h"
#include "tm1640_config.h"
#include "ui_manager.h"
#include <string.h>

// 内部缓冲区尺寸
#define DISPLAY_WIDTH   MATRIX_COLS
#define DISPLAY_HEIGHT  MATRIX_ROWS

// 滚动缓冲区尺寸（水平滚动需要更宽）
#define SCROLL_BUFFER_WIDTH   (DISPLAY_WIDTH * 20)  // 最大允许的滚动宽度，可容纳约16个字符
#define SCROLL_BUFFER_HEIGHT  DISPLAY_HEIGHT

// 显示缓冲区（当前可见区域）
static uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];

// 滚动缓冲区（虚拟大缓冲区）
static uint8_t scroll_buffer[SCROLL_BUFFER_HEIGHT][SCROLL_BUFFER_WIDTH];

// 当前配置
static DisplayConfig current_config = {
    .mode = DISPLAY_MODE_STATIC,
    .brightness = TM1640_DEFAULT_BRIGHTNESS,
    .scroll_speed_ms = 100,
    .auto_clear = true,
    .invert_display = false
};

// 滚动状态
static struct {
    bool active;
    bool horizontal;
    bool direction_left;  // 水平方向：true=左，false=右
    bool direction_up;    // 垂直方向：true=上，false=下
    uint16_t speed_ms;
    uint16_t offset;
    uint16_t max_offset;
    uint32_t last_update;
} scroll_state = {0};

 
// 将显示缓冲区复制到LED矩阵
static void buffer_to_matrix(void) {
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            bool state = display_buffer[y][x] != 0;
            if (current_config.invert_display) {
                state = !state;
            }
            led_matrix_set_pixel(y, x, state);
        }
    }
    led_matrix_refresh();
}

static void buffer_to_matrix_notrefresh(void) {
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            bool state = display_buffer[y][x] != 0;
            if (current_config.invert_display) {
                state = !state;
            }
            led_matrix_set_pixel(y, x, state);
        }
    }
    led_cache_refresh();
}

// 从滚动缓冲区提取当前可见区域
static void update_from_scroll_buffer(void) {
    if (scroll_state.horizontal) {
        // 水平滚动
        for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
            for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
                uint16_t src_x = scroll_state.offset + x;
                if (src_x < SCROLL_BUFFER_WIDTH) {
                    display_buffer[y][x] = scroll_buffer[y][src_x];
                } else {
                    display_buffer[y][x] = 0;
                }
            }
        }
    } else {
        // 垂直滚动
        for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
            uint16_t src_y = scroll_state.offset + y;
            for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
                if (src_y < SCROLL_BUFFER_HEIGHT) {
                    display_buffer[y][x] = scroll_buffer[src_y][x];
                } else {
                    display_buffer[y][x] = 0;
                }
            }
        }
    }
}

// 初始化显示系统
void display_init(void) {
    // 初始化LED矩阵
    led_matrix_init();

    // 初始化动画系统
    animation_init();

    // 清空缓冲区
    memset(display_buffer, 0, sizeof(display_buffer));
    memset(scroll_buffer, 0, sizeof(scroll_buffer));
 
    // 初始显示
    buffer_to_matrix();
	  ui_manager_init();
    
	  ui_fileUI_Iteam();
	  ui_manager_refresh_current();
	 
	   
}

// 配置显示
void display_set_config(DisplayConfig *config) {
    if (config == NULL) return;
    memcpy(&current_config, config, sizeof(DisplayConfig));

    // 应用亮度设置
    // TM1640亮度控制需要调用tm1640_set_brightness，但这里无法直接访问
    // 可以通过LED矩阵模块添加亮度控制接口，暂时跳过
}

void display_get_config(DisplayConfig *config) {
    if (config == NULL) return;
    memcpy(config, &current_config, sizeof(DisplayConfig));
}

// 清屏
void display_clear(void) {
    memset(display_buffer, 0, sizeof(display_buffer));
    memset(scroll_buffer, 0, sizeof(scroll_buffer));
    scroll_state.active = false;
    buffer_to_matrix();
}

// 开启显示
void display_on(void) {
    // TM1640显示开启已在初始化时设置
    current_config.mode = DISPLAY_MODE_STATIC;
}

// 关闭显示
void display_off(void) {
    led_matrix_clear_all();
    led_matrix_refresh();
    current_config.mode = DISPLAY_MODE_OFF;
}

// 设置亮度
void display_set_brightness(uint8_t brightness) {
    if (brightness > 7) brightness = 7;
    current_config.brightness = brightness;
    // 需要调用TM1640亮度设置，暂时无法直接访问
}

// 设置像素
void display_set_pixel(uint8_t x, uint8_t y, bool state) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    display_buffer[y][x] = state ? 1 : 0;
    buffer_to_matrix();
}

void display_load_pixel(uint8_t x, uint8_t y, bool state)
{ 
     if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;
    display_buffer[y][x] = state ? 1 : 0;
	  buffer_to_matrix_notrefresh();
}
// 绘制直线
void display_draw_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, bool state) {
    // 使用LED矩阵的绘制函数，但需要将坐标转换到显示缓冲区
    // 简单实现：直接调用LED矩阵函数，然后同步缓冲区
    led_matrix_draw_line(x1, y1, x2, y2, state);

    // 同步缓冲区：从LED矩阵获取当前状态
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            display_buffer[y][x] = led_matrix_get_pixel(y, x) ? 1 : 0;
        }
    }
}

// 绘制矩形
void display_draw_rect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, bool state, bool fill) {
    led_matrix_draw_rect(x, y, width, height, state, fill);

    // 同步缓冲区
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            display_buffer[y][x] = led_matrix_get_pixel(y, x) ? 1 : 0;
        }
    }
}

// 绘制圆
void display_draw_circle(uint8_t x, uint8_t y, uint8_t radius, bool state) {
    led_matrix_draw_circle(x, y, radius, state);

    // 同步缓冲区
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            display_buffer[y][x] = led_matrix_get_pixel(y, x) ? 1 : 0;
        }
    }
}

// 渲染一个字符到显示缓冲区
static void render_char_to_buffer(char c, uint8_t x, uint8_t y) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    const uint8_t *char_data = font_get_char_data(c);

    // 字体尺寸：5列7行
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t col_x = x + col;
        if (col_x >= DISPLAY_WIDTH) break;

        uint8_t col_data = char_data[col];
        // 每列数据：位0对应顶部行（行0）
        for (uint8_t row = 0; row < 7; row++) {
            uint8_t row_y = y + row;
            if (row_y >= DISPLAY_HEIGHT) break;

            if (col_data & (1 << row)) {
                display_buffer[row_y][col_x] = 1;
            }
        }
    }
}

// 显示文本
void display_text(const char *text, uint8_t x, uint8_t y, Alignment h_align, Alignment v_align) {
    if (text == NULL) return;

    // 计算文本宽度和高度
    uint8_t text_width = font_get_string_width(text);
    uint8_t text_height = font_get_char_height();

    // 水平对齐
    switch (h_align) {
        case ALIGN_CENTER:
            if (x + text_width > DISPLAY_WIDTH) {
                x = 0;
            } else {
                x = (DISPLAY_WIDTH - text_width) / 2;
            }
            break;
        case ALIGN_RIGHT:
            if (text_width <= DISPLAY_WIDTH) {
                x = DISPLAY_WIDTH - text_width;
            } else {
                x = 0;
            }
            break;
        case ALIGN_LEFT:
        default:
            x = 0;
            break;
    }

    // 垂直对齐
    switch (v_align) {
        case ALIGN_CENTER:
            if (y + text_height > DISPLAY_HEIGHT) {
                y = 0;
            } else {
                y = (DISPLAY_HEIGHT - text_height) / 2;
            }
            break;
        case ALIGN_BOTTOM:
            if (text_height <= DISPLAY_HEIGHT) {
                y = DISPLAY_HEIGHT - text_height;
            } else {
                y = 0;
            }
            break;
        case ALIGN_TOP:
        default:
            y = 0;
            break;
    }

    // 清空显示缓冲区
    memset(display_buffer, 0, sizeof(display_buffer));

    // 绘制文本：由于矩阵只有5列宽，只能显示一个字符
    // 显示第一个字符，或使用滚动显示更长文本
    if (strlen(text) > 0) {
        render_char_to_buffer(text[0], x, y);
    }

    buffer_to_matrix();
}

// 居中显示文本
void display_text_centered(const char *text) {
    display_text(text, 0, 0, ALIGN_CENTER, ALIGN_CENTER);
}

// 滚动显示文本
void display_text_scrolling(const char *text, DisplayMode scroll_direction, uint16_t speed_ms) {
    if (text == NULL) return;

    // 计算文本宽度和长度
    uint8_t text_width = font_get_string_width(text);
    uint8_t text_len = strlen(text);

    // 如果只有一个字符，不进行滚动，静态显示
    if (text_len < 2) {
        display_text_centered(text);
        return;
    }

    // 清空滚动缓冲区
    memset(scroll_buffer, 0, sizeof(scroll_buffer));

    // 将文本绘制到滚动缓冲区
    uint8_t x_pos = 0;
    uint8_t y_pos = 0;  // 垂直居中：矩阵高度7，字体高度7，所以从顶部开始

    // 遍历文本中的每个字符
    while (*text && x_pos < SCROLL_BUFFER_WIDTH) {
        char c = *text;
        const uint8_t *char_data = font_get_char_data(c);

        // 字体尺寸：5列7行
        for (uint8_t col = 0; col < 5; col++) {
            uint8_t col_x = x_pos + col;
            if (col_x >= SCROLL_BUFFER_WIDTH) break;

            uint8_t col_data = char_data[col];
            // 每列数据：位0对应顶部行（行0）
            for (uint8_t row = 0; row < 7; row++) {
                uint8_t row_y = y_pos + row;
                if (row_y >= DISPLAY_HEIGHT) break;

                if (col_data & (1 << row)) {
                    scroll_buffer[row_y][col_x] = 1;
                }
            }
        }

        x_pos += 5 + 1;  // 字符宽度5 + 1像素间距
        text++;
    }

    // 设置滚动状态
    scroll_state.active = true;
    scroll_state.horizontal = (scroll_direction == DISPLAY_MODE_HORIZONTAL);
    scroll_state.direction_left = true;
    scroll_state.direction_up = true;
    scroll_state.speed_ms = speed_ms;
    scroll_state.offset = 0;
 
		if (scroll_state.horizontal) {
        // 水平滚动：根据文本宽度计算最大偏移量
        if (text_width > DISPLAY_WIDTH) {
            scroll_state.max_offset = text_width - DISPLAY_WIDTH;
        } else {
            // 文本较短，至少滚动整个显示宽度，确保文本从右侧进入，左侧消失
            scroll_state.max_offset = DISPLAY_WIDTH;
        }
    } else {
        // 垂直滚动：使用固定偏移量（文本高度7像素）
        scroll_state.max_offset = 7;
    }
    scroll_state.last_update = HAL_GetTick(); // 需要系统时间

    current_config.mode = scroll_direction;
}

// 开始水平滚动
void display_start_horizontal_scroll(bool direction_left, uint16_t speed_ms) {
    scroll_state.active = true;
    scroll_state.horizontal = true;
    scroll_state.direction_left = direction_left;
    scroll_state.speed_ms = speed_ms;
    scroll_state.offset = 0;
    scroll_state.max_offset = SCROLL_BUFFER_WIDTH - DISPLAY_WIDTH;
    scroll_state.last_update = HAL_GetTick();

    current_config.mode = DISPLAY_MODE_HORIZONTAL;
}

// 开始垂直滚动
void display_start_vertical_scroll(bool direction_up, uint16_t speed_ms) {
    scroll_state.active = true;
    scroll_state.horizontal = false;
    scroll_state.direction_up = direction_up;
    scroll_state.speed_ms = speed_ms;
    scroll_state.offset = 0;
    scroll_state.max_offset = SCROLL_BUFFER_HEIGHT - DISPLAY_HEIGHT;
    scroll_state.last_update = HAL_GetTick();

    current_config.mode = DISPLAY_MODE_VERTICAL;
}

// 停止滚动
void display_stop_scroll(void) {
    scroll_state.active = false;
    current_config.mode = DISPLAY_MODE_STATIC;
}

// 播放开机动画
void display_play_power_on_animation(void) {
    animation_power_on();
    current_config.mode = DISPLAY_MODE_ANIMATION;
}

// 播放关机动画
void display_play_power_off_animation(void) {
    animation_power_off();
    current_config.mode = DISPLAY_MODE_ANIMATION;
}

// 播放UI切换动画
void display_play_ui_transition(bool direction_left) {
    animation_ui_transition(direction_left ? DIRECTION_LEFT : DIRECTION_RIGHT,
                           UI_TRANSITION_DURATION_MS);
    current_config.mode = DISPLAY_MODE_ANIMATION;
}

// 获取显示缓冲区
void display_get_buffer(uint8_t *buffer, uint16_t size) {
    if (buffer == NULL || size < sizeof(display_buffer)) return;
    memcpy(buffer, display_buffer, sizeof(display_buffer));
}

// 设置显示缓冲区
void display_set_buffer(uint8_t *buffer, uint16_t size) {
    if (buffer == NULL || size < sizeof(display_buffer)) return;
    memcpy(display_buffer, buffer, sizeof(display_buffer));
    buffer_to_matrix();
}

// 更新显示（处理滚动等）
void display_update(void) {
    // 更新动画系统
     animation_update();

    // 处理滚动
    if (scroll_state.active && current_config.mode != DISPLAY_MODE_ANIMATION) {
        uint32_t current_time = HAL_GetTick();
        if (current_time - scroll_state.last_update >= scroll_state.speed_ms) {
            scroll_state.last_update = current_time;

            // 更新滚动偏移
            if (scroll_state.horizontal) {
                if (scroll_state.direction_left) {
                    scroll_state.offset++;
                    if (scroll_state.offset >= scroll_state.max_offset) {
                        scroll_state.offset = 0;
										  
                    }
                } else {
                    if (scroll_state.offset == 0) {
                        scroll_state.offset = scroll_state.max_offset;
											   
                    }
                    scroll_state.offset--;
                }
            } else {
                if (scroll_state.direction_up) {
                    scroll_state.offset++;
                    if (scroll_state.offset >= scroll_state.max_offset) {
                        scroll_state.offset = 0;
                    }
                } else {
                    if (scroll_state.offset == 0) {
                        scroll_state.offset = scroll_state.max_offset;
                    }
                    scroll_state.offset--;
                }
            }

            // 更新显示缓冲区
            update_from_scroll_buffer();
            buffer_to_matrix();
        }
    }
}

// 测试图案
void display_test_pattern(void) {
    led_matrix_test_pattern();

    // 同步缓冲区
    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            display_buffer[y][x] = led_matrix_get_pixel(y, x) ? 1 : 0;
        }
    }
}

// 测试文本
void display_test_text(void) {
    display_clear();
    display_text_centered("TEST");
} 
void matrix_callback(void *arg)
{
		display_update();
	  
}
