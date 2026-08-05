#include "animation.h"
#include "led_matrix.h"
#include "tm1640_config.h"
#include <string.h>
#include "btim.h"
//#define ANIMATION_GET_TICK() getTim6Tick()
/* 系统滴答函数（用户需要提供，默认为HAL_GetTick） */
#ifndef ANIMATION_GET_TICK
#include "stm32f1xx_hal.h"
#define ANIMATION_GET_TICK() HAL_GetTick()
#endif

/* 蜂鸣器回调函数指针 */
static BuzzerCallback buzzer_power_on_cb = NULL;
static BuzzerCallback buzzer_power_off_cb = NULL;

/* 当前动画控制 */
static AnimationControl current_animation = {0};

/* 自定义动画帧数据 */
static uint8_t *custom_frame_data = NULL;
static uint8_t custom_frame_count = 0;
//static uint8_t custom_frame_width = 0;
//static uint8_t custom_frame_height = 0;

/* 动画帧缓冲区 */
static uint8_t animation_frame_buffer[MATRIX_ROWS][MATRIX_COLS] = {0};
static uint8_t animation_frame_buffer2[MATRIX_ROWS][MATRIX_COLS] = {0};
static bool animation_frame_captured = false;
static bool animation_frame_captured2 = false;

/* 开机动画帧数据（7x5，每帧一个位图） */
/* 帧数：5帧（从中心向外扩散） */
static const uint8_t power_on_frames[5][7] = {
    /* 帧1：中心点 */
    {0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x04,  /* 00100 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00}, /* 00000 */
    /* 帧2：小十字 */
    {0x00,  /* 00000 */
     0x04,  /* 00100 */
     0x0E,  /* 01110 */
     0x04,  /* 00100 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00}, /* 00000 */
    /* 帧3：大十字 */
    {0x04,  /* 00100 */
     0x0E,  /* 01110 */
     0x1F,  /* 11111 */
     0x0E,  /* 01110 */
     0x04,  /* 00100 */
     0x00,  /* 00000 */
     0x00}, /* 00000 */
    /* 帧4：填充中间 */
    {0x0E,  /* 01110 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x0E,  /* 01110 */
     0x00,  /* 00000 */
     0x00}, /* 00000 */
    /* 帧5：全亮 */
    {0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F}  /* 11111 */
};

/* 开机动画每帧持续时间（毫秒），实现慢速扩充2下后快速填满 */
static const uint16_t power_on_frame_durations[5] = {300, 300, 100, 100, 100};

/* 关机动画帧数据（7x5，从外向内收缩） */
static const uint8_t power_off_frames[5][7] = {
    /* 帧1：全亮 */
    {0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F,  /* 11111 */
     0x1F}, /* 11111 */
    /* 帧2：去掉边框 */
    {0x1F,  /* 11111 */
     0x11,  /* 10001 */
     0x11,  /* 10001 */
     0x11,  /* 10001 */
     0x11,  /* 10001 */
     0x11,  /* 10001 */
     0x1F}, /* 11111 */
    /* 帧3：更小 */
    {0x00,  /* 00000 */
     0x0E,  /* 01110 */
     0x0E,  /* 01110 */
     0x0E,  /* 01110 */
     0x0E,  /* 01110 */
     0x0E,  /* 01110 */
     0x00}, /* 00000 */
    /* 帧4：小十字 */
    {0x00,  /* 00000 */
     0x04,  /* 00100 */
     0x0E,  /* 01110 */
     0x04,  /* 00100 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00}, /* 00000 */
    /* 帧5：中心点 */
    {0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x04,  /* 00100 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00,  /* 00000 */
     0x00}  /* 00000 */
};

// 将位图数据加载到帧缓冲区
static void load_frame_to_buffer(const uint8_t frame[MATRIX_ROWS]) {
    led_matrix_clear_all();

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        uint8_t row_data = frame[row];
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (row_data & (1 << (MATRIX_COLS - 1 - col))) {  // 注意位顺序，最高位对应最左边
                led_matrix_set_pixel(row, col, true);
            }
        }
    }

    led_matrix_refresh();
}

// 渲染垂直滚动帧
static void render_vertical_scroll_frame(uint8_t frame, AnimationDirection direction) {
    led_matrix_clear_all();

    if (!animation_frame_captured) return;

    // 计算垂直偏移：总帧数 = MATRIX_ROWS * 2，偏移范围从0到MATRIX_ROWS
    // 使用浮点计算，但用整数近似：偏移 = frame * MATRIX_ROWS / (MATRIX_ROWS * 2) = frame / 2
    int8_t offset = frame / 2; // 0到6（当frame=12时，offset=6）
    if (offset > MATRIX_ROWS) offset = MATRIX_ROWS;

    // 根据方向调整：向上滚动时偏移为正（内容上移），向下滚动时偏移为负（内容下移）
    // 但我们的缓冲区只存储原始图像，所以向上滚动时显示原始图像向上偏移
    // 向下滚动时显示原始图像向下偏移，但动画开始时图像在屏幕外顶部，所以需要调整

    if (direction == DIRECTION_UP) {
        // 向上滚动：图像向上移动，所以源行 = row - offset
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            int8_t src_row = row - offset;
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                if (src_row >= 0 && src_row < MATRIX_ROWS) {
                    bool pixel_state = animation_frame_buffer[src_row][col] != 0;
                    led_matrix_set_pixel(row, col, pixel_state);
                }
                // 否则像素保持关闭（图像移出屏幕）
            }
        }
    } else { // DIRECTION_DOWN
        // 向下滚动：图像从上方进入，所以源行 = row + (MATRIX_ROWS - offset)
        // 当offset=0时，图像完全在屏幕外顶部；当offset=MATRIX_ROWS时，图像在正常位置
        int8_t start_offset = MATRIX_ROWS - offset;
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            int8_t src_row = row + start_offset;
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                if (src_row >= 0 && src_row < MATRIX_ROWS) {
                    bool pixel_state = animation_frame_buffer[src_row][col] != 0;
                    led_matrix_set_pixel(row, col, pixel_state);
                }
                // 否则像素保持关闭
            }
        }
    }

    led_matrix_refresh();
}

// 渲染UI切换过渡帧
static void render_ui_transition_frame(uint8_t frame, AnimationDirection direction) {
    led_matrix_clear_all();

    if (!animation_frame_captured) return;

    // 计算水平偏移：总帧数 = 10，偏移范围从0到MATRIX_COLS
    // 偏移 = frame * MATRIX_COLS / 10
    int8_t offset = frame * MATRIX_COLS / 10;
    if (offset > MATRIX_COLS) offset = MATRIX_COLS;

    // 根据方向调整
    if (direction == DIRECTION_LEFT) {
        // 向左切换：当前图像向左滑出，新图像从右侧进入
        // 我们只存储当前图像，所以显示当前图像向左偏移
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                int8_t src_col = col + offset; // 图像向左移动，所以源列在右侧
                if (src_col >= 0 && src_col < MATRIX_COLS) {
                    bool pixel_state = animation_frame_buffer[row][src_col] != 0;
                    led_matrix_set_pixel(row, col, pixel_state);
                }
                // 否则像素保持关闭（图像移出屏幕）
            }
        }
    } else { // DIRECTION_RIGHT
        // 向右切换：当前图像向右滑出，新图像从左侧进入
        // 显示当前图像向右偏移
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                int8_t src_col = col - offset;
                if (src_col >= 0 && src_col < MATRIX_COLS) {
                    bool pixel_state = animation_frame_buffer[row][src_col] != 0;
                    led_matrix_set_pixel(row, col, pixel_state);
                }
                // 否则像素保持关闭
            }
        }
    }

    led_matrix_refresh();
}

// 渲染UI缩放动画帧（带滑动效果）
static void render_ui_zoom_frame(uint8_t frame, AnimationDirection direction) {
    led_matrix_clear_all();

    if (!animation_frame_captured || !animation_frame_captured2) return;

    // 总帧数：10帧
    // 前5帧：当前图案缩小消失 (帧0-4)
    // 后5帧：新图案放大出现 (帧5-9)

    // 计算滑动偏移：范围从0到MATRIX_COLS（全屏宽度）
    // 对于当前图案（帧0-4）：偏移从0增加到MATRIX_COLS（滑出屏幕）
    // 对于新图案（帧5-9）：偏移从MATRIX_COLS减少到0（滑入屏幕）
    int8_t slide_offset;
    if (frame < 5) {
        // 当前图案：滑动偏移从0增加到MATRIX_COLS
        slide_offset = frame * MATRIX_COLS / 5;  // 0, 1, 2, 3, 4 (当MATRIX_COLS=5)
    } else {
        // 新图案：滑动偏移从MATRIX_COLS减少到0
        slide_offset = (9 - frame) * MATRIX_COLS / 5;  // 4, 3, 2, 1, 0
    }

    // 根据方向调整滑动偏移的正负
    if (direction == DIRECTION_LEFT) {
        // 向左切换：当前图案向左滑出（负偏移），新图案从右侧滑入（正偏移）
        // 但我们的slide_offset总是正数，所以需要根据图案调整方向
        if (frame < 5) {
            // 当前图案向左滑出：偏移为负
            slide_offset = -slide_offset;
        } else {
            // 新图案从右侧滑入：偏移为正（从右侧进入，所以开始时在屏幕右侧）
            // 新图案开始时应该在屏幕右侧（正偏移），然后移动到中心（0偏移）
            // 所以偏移应该为正，并逐渐减少到0
            // slide_offset已经是正数并减少到0，符合要求
        }
    } else { // DIRECTION_RIGHT
        // 向右切换：当前图案向右滑出（正偏移），新图案从左侧滑入（负偏移）
        if (frame < 5) {
            // 当前图案向右滑出：偏移为正
            // slide_offset已经是正数，符合要求
        } else {
            // 新图案从左侧滑入：偏移为负（从左侧进入，所以开始时在屏幕左侧）
            slide_offset = -slide_offset;
        }
    }

    if (frame < 5) {
        // 当前图案缩小：缩放百分比从100%到20% (5帧：100%, 80%, 60%, 40%, 20%)
        uint8_t scale_percent = 100 - frame * 20; // 100, 80, 60, 40, 20

        // 计算缩放后的尺寸（最小为1）
        int8_t scaled_width = (MATRIX_COLS * scale_percent) / 100;
        int8_t scaled_height = (MATRIX_ROWS * scale_percent) / 100;
        if (scaled_width < 1) scaled_width = 1;
        if (scaled_height < 1) scaled_height = 1;

        // 计算偏移以使图案居中
        int8_t offset_x = (MATRIX_COLS - scaled_width) / 2;
        int8_t offset_y = (MATRIX_ROWS - scaled_height) / 2;

        // 添加滑动偏移
        offset_x += slide_offset;

        // 绘制缩放后的当前图案
        for (int8_t y = 0; y < scaled_height; y++) {
            for (int8_t x = 0; x < scaled_width; x++) {
                // 映射回原始坐标（使用整数运算）
                int8_t src_x = (x * MATRIX_COLS) / scaled_width;
                int8_t src_y = (y * MATRIX_ROWS) / scaled_height;
                if (src_x >= 0 && src_x < MATRIX_COLS && src_y >= 0 && src_y < MATRIX_ROWS) {
                    bool pixel_state = animation_frame_buffer[src_y][src_x] != 0;
                    // 检查目标位置是否在屏幕范围内
                    int8_t dest_x = offset_x + x;
                    int8_t dest_y = offset_y + y;
                    if (dest_x >= 0 && dest_x < MATRIX_COLS && dest_y >= 0 && dest_y < MATRIX_ROWS) {
                        led_matrix_set_pixel(dest_y, dest_x, pixel_state);
                    }
                }
            }
        }
    } else {
        // 新图案放大：缩放百分比从20%到100% (5帧：20%, 40%, 60%, 80%, 100%)
        uint8_t scale_percent = (frame - 5) * 20 + 20; // 20, 40, 60, 80, 100

        // 计算缩放后的尺寸
        int8_t scaled_width = (MATRIX_COLS * scale_percent) / 100;
        int8_t scaled_height = (MATRIX_ROWS * scale_percent) / 100;
        if (scaled_width < 1) scaled_width = 1;
        if (scaled_height < 1) scaled_height = 1;

        // 计算偏移以使图案居中
        int8_t offset_x = (MATRIX_COLS - scaled_width) / 2;
        int8_t offset_y = (MATRIX_ROWS - scaled_height) / 2;

        // 添加滑动偏移
        offset_x += slide_offset;

        // 绘制缩放后的新图案
        for (int8_t y = 0; y < scaled_height; y++) {
            for (int8_t x = 0; x < scaled_width; x++) {
                // 映射回原始坐标
                int8_t src_x = (x * MATRIX_COLS) / scaled_width;
                int8_t src_y = (y * MATRIX_ROWS) / scaled_height;
                if (src_x >= 0 && src_x < MATRIX_COLS && src_y >= 0 && src_y < MATRIX_ROWS) {
                    bool pixel_state = animation_frame_buffer2[src_y][src_x] != 0;
                    // 检查目标位置是否在屏幕范围内
                    int8_t dest_x = offset_x + x;
                    int8_t dest_y = offset_y + y;
                    if (dest_x >= 0 && dest_x < MATRIX_COLS && dest_y >= 0 && dest_y < MATRIX_ROWS) {
                        led_matrix_set_pixel(dest_y, dest_x, pixel_state);
                    }
                }
            }
        }
    }

    led_matrix_refresh();
}

// 初始化动画系统
void animation_init(void) {
    memset(&current_animation, 0, sizeof(current_animation));
    current_animation.state = ANIMATION_STOPPED;
}

// 注册蜂鸣器回调
void animation_register_buzzer_callback(BuzzerCallback power_on_cb, BuzzerCallback power_off_cb) {
    buzzer_power_on_cb = power_on_cb;
    buzzer_power_off_cb = power_off_cb;
}

// 开始动画
void animation_start(AnimationType type, AnimationDirection direction, uint32_t duration_ms) {
    if (current_animation.state == ANIMATION_RUNNING) {
        animation_stop();
    }

    // 重置第二个缓冲区标志（UI缩放动画由调用者手动设置缓冲区）
    if (type != ANIMATION_UI_ZOOM) {
        animation_frame_captured2 = false;
    }

    current_animation.type = type;
    current_animation.direction = direction;
    current_animation.duration_ms = duration_ms;
    current_animation.start_time = ANIMATION_GET_TICK();
    current_animation.state = ANIMATION_RUNNING;
    current_animation.current_frame = 0;
    current_animation.repeat = false;
    current_animation.repeat_counter = 0;

    // 根据动画类型设置帧参数
    switch (type) {
        case ANIMATION_POWER_ON:
            current_animation.total_frames = 5;
            current_animation.frame_interval_ms = 0; // 标志：使用可变帧间隔
            // 计算总持续时间（各帧持续时间之和）
            current_animation.duration_ms = 0;
            for (uint8_t i = 0; i < 5; i++) {
                current_animation.duration_ms += power_on_frame_durations[i];
            }
            if (buzzer_power_on_cb != NULL) {
                buzzer_power_on_cb();
            }
            break;
        case ANIMATION_POWER_OFF:
            current_animation.total_frames = 5;
            current_animation.frame_interval_ms = ANIMATION_FRAME_INTERVAL_MS;
            current_animation.duration_ms = current_animation.frame_interval_ms * current_animation.total_frames;
            if (buzzer_power_off_cb != NULL) {
                buzzer_power_off_cb();
            }
            break;
        case ANIMATION_HORIZONTAL_SCROLL:
            current_animation.total_frames = MATRIX_COLS * 2; // 粗略估计
            current_animation.frame_interval_ms = 100; // 默认100ms每帧
            break;
        case ANIMATION_VERTICAL_SCROLL:
            current_animation.total_frames = MATRIX_ROWS * 2;
            // 每帧间隔 = 总持续时间 / 总帧数
            if (current_animation.total_frames > 0) {
                current_animation.frame_interval_ms = duration_ms / current_animation.total_frames;
                if (current_animation.frame_interval_ms < 10) current_animation.frame_interval_ms = 10;
            } else {
                current_animation.frame_interval_ms = 100;
            }
            break;
        case ANIMATION_UI_TRANSITION:
            current_animation.total_frames = 10; // 10帧过渡
            current_animation.frame_interval_ms = duration_ms / 10;
            break;
        case ANIMATION_UI_ZOOM:
            current_animation.total_frames = 10; // 10帧缩放动画
            current_animation.frame_interval_ms = duration_ms / 10;
            break;
        default:
            current_animation.total_frames = 1;
            current_animation.frame_interval_ms = duration_ms;
            break;
    }

    // 为需要当前帧的动画捕获显示内容
    // 注意：ANIMATION_UI_ZOOM由调用者手动设置缓冲区，不在这里捕获
    if (type == ANIMATION_VERTICAL_SCROLL || type == ANIMATION_UI_TRANSITION) {
        // 获取当前LED矩阵状态
        FrameBuffer current_frame;
        led_matrix_get_buffer(&current_frame);
        // 复制到动画缓冲区
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                animation_frame_buffer[row][col] = current_frame[row][col];
            }
        }
        animation_frame_captured = true;
    } else if (type != ANIMATION_UI_ZOOM) {
        // 对于非UI缩放动画，重置捕获标志
        // UI缩放动画的缓冲区由调用者手动设置
        animation_frame_captured = false;
    }

    // 立即显示第一帧
    animation_update();
}

// 停止动画
void animation_stop(void) {
    current_animation.state = ANIMATION_STOPPED;
    led_matrix_clear_all();
    led_matrix_refresh();
}

// 暂停动画
void animation_pause(void) {
    if (current_animation.state == ANIMATION_RUNNING) {
        current_animation.state = ANIMATION_PAUSED;
    }
}

// 恢复动画
void animation_resume(void) {
    if (current_animation.state == ANIMATION_PAUSED) {
        current_animation.state = ANIMATION_RUNNING;
        // 调整开始时间，使得暂停时间不影响动画
        uint32_t paused_duration = ANIMATION_GET_TICK() - current_animation.start_time;
        current_animation.start_time += paused_duration;
    }
}

// 检查是否正在运行
bool animation_is_running(void) {
    return current_animation.state == ANIMATION_RUNNING;
}

// 获取动画状态
AnimationState animation_get_state(void) {
    return current_animation.state;
}

// 开机动画
void animation_power_on(void) {
    animation_start(ANIMATION_POWER_ON, DIRECTION_LEFT, POWER_ON_ANIMATION_DURATION_MS);
}

// 关机动画
void animation_power_off(void) {
    animation_start(ANIMATION_POWER_OFF, DIRECTION_LEFT, POWER_OFF_ANIMATION_DURATION_MS);
}

// 横屏滚动
void animation_horizontal_scroll(AnimationDirection direction, uint32_t speed_ms) {
    animation_start(ANIMATION_HORIZONTAL_SCROLL, direction, speed_ms * MATRIX_COLS);
}

// 竖屏滚动
void animation_vertical_scroll(AnimationDirection direction, uint32_t speed_ms) {
    animation_start(ANIMATION_VERTICAL_SCROLL, direction, speed_ms * MATRIX_ROWS);
}

// UI切换动画
void animation_ui_transition(AnimationDirection direction, uint32_t duration_ms) {
    animation_start(ANIMATION_UI_TRANSITION, direction, duration_ms);
}

// UI缩放动画
void animation_ui_zoom(AnimationDirection direction, uint32_t duration_ms) {
    animation_start(ANIMATION_UI_ZOOM, direction, duration_ms);
}

// 设置缩放动画的当前帧缓冲区（当前图案）
void animation_set_current_frame_buffer(uint8_t buffer[MATRIX_ROWS][MATRIX_COLS]) {
    if (buffer == NULL) return;
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            animation_frame_buffer[row][col] = buffer[row][col];
        }
    }
    animation_frame_captured = true;
}

// 设置缩放动画的下一个帧缓冲区（新图案）
void animation_set_next_frame_buffer(uint8_t buffer[MATRIX_ROWS][MATRIX_COLS]) {
    if (buffer == NULL) return;
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            animation_frame_buffer2[row][col] = buffer[row][col];
        }
    }
    animation_frame_captured2 = true;
}

// 设置自定义动画帧
void animation_set_custom_frames(uint8_t *frame_data, uint8_t frame_count, uint8_t frame_width, uint8_t frame_height) {
    custom_frame_data = frame_data;
    custom_frame_count = frame_count;
 //   custom_frame_width = frame_width;
//    custom_frame_height = frame_height;
}

// 开始自定义动画
void animation_start_custom(AnimationDirection direction, uint32_t duration_ms, bool repeat) {
    if (custom_frame_data == NULL || custom_frame_count == 0) return;

    if (current_animation.state == ANIMATION_RUNNING) {
        animation_stop();
    }

    current_animation.type = ANIMATION_CUSTOM;
    current_animation.direction = direction;
    current_animation.duration_ms = duration_ms;
    current_animation.start_time = ANIMATION_GET_TICK();
    current_animation.state = ANIMATION_RUNNING;
    current_animation.current_frame = 0;
    current_animation.total_frames = custom_frame_count;
    current_animation.frame_interval_ms = duration_ms / custom_frame_count;
    current_animation.repeat = repeat;
    current_animation.repeat_counter = 0;

    animation_update();
}

// 动画帧更新
void animation_update(void) {
    if (current_animation.state != ANIMATION_RUNNING) {
        return;
    }

    uint32_t current_time = ANIMATION_GET_TICK();
    uint32_t elapsed = current_time - current_animation.start_time;

    // 计算当前帧
    uint8_t frame = 0;
    if (current_animation.type == ANIMATION_POWER_ON && current_animation.frame_interval_ms == 0) {
        // 使用可变帧间隔：根据每帧持续时间计算当前帧
        uint32_t accumulated = 0;
        for (frame = 0; frame < current_animation.total_frames; frame++) {
            accumulated += power_on_frame_durations[frame];
            if (elapsed < accumulated) {
                break;
            }
        }
        // 如果elapsed超过总时间，frame可能等于total_frames，需要限制
        if (frame >= current_animation.total_frames) {
            frame = current_animation.total_frames - 1;
        }
    } else {
        // 固定帧间隔
        frame = (elapsed / current_animation.frame_interval_ms) % current_animation.total_frames;
    }

    // 如果帧发生变化，更新显示
    if (frame != current_animation.current_frame) {
        current_animation.current_frame = frame;

        // 根据动画类型显示相应帧
        switch (current_animation.type) {
            case ANIMATION_POWER_ON:
                if (frame < 5) {
                    load_frame_to_buffer(power_on_frames[frame]);
                }
                break;
            case ANIMATION_POWER_OFF:
                if (frame < 5) {
                    load_frame_to_buffer(power_off_frames[frame]);
                }
                break;
            case ANIMATION_HORIZONTAL_SCROLL:
                // 横屏滚动需要在外部处理，这里只管理状态
                break;
            case ANIMATION_VERTICAL_SCROLL:
                // 竖屏滚动：渲染当前帧
                render_vertical_scroll_frame(frame, current_animation.direction);
                break;
            case ANIMATION_UI_TRANSITION:
                // UI切换：渲染当前帧
                render_ui_transition_frame(frame, current_animation.direction);
                break;
            case ANIMATION_UI_ZOOM:
                // UI缩放动画：渲染当前帧
                render_ui_zoom_frame(frame, current_animation.direction);
                break;
            case ANIMATION_CUSTOM:
                // 自定义动画：需要外部处理帧显示
                break;
            default:
                break;
        }

        // 检查动画是否完成
        if (frame == current_animation.total_frames - 1) {
            if (current_animation.repeat) {
                // 重复动画：重置开始时间
                current_animation.start_time = current_time;
                current_animation.repeat_counter++;
            } else {
                // 动画完成
                current_animation.state = ANIMATION_COMPLETED;
            }
        }
    }

    // 检查总持续时间是否结束
    if (elapsed >= current_animation.duration_ms && !current_animation.repeat) {
        current_animation.state = ANIMATION_COMPLETED;
        // 对于UI缩放动画，不清除屏幕（最后一帧已经显示完整新图案）
        if (current_animation.type != ANIMATION_UI_ZOOM) {
            led_matrix_clear_all();
            led_matrix_refresh();
        }
    }
}
