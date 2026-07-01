#ifndef __TM1640_CONFIG_H
#define __TM1640_CONFIG_H

#include "stm32f1xx_hal.h"

// ============================================================================
// TM1640 硬件引脚配置
// ============================================================================

// CLK引脚配置
#define TM1640_CLK_PORT     GPIOB
#define TM1640_CLK_PIN      GPIO_PIN_6

// DIN引脚配置
#define TM1640_DIN_PORT     GPIOB
#define TM1640_DIN_PIN      GPIO_PIN_7

// 默认亮度 (0-7)
#define TM1640_DEFAULT_BRIGHTNESS  7

// ============================================================================
// LED矩阵配置 (7行 x 5列)
// ============================================================================

// 矩阵尺寸
#define MATRIX_ROWS     7
#define MATRIX_COLS     5

// TM1640的GRID和SEG映射
// TM1640有8个GRID (0-7) 和16个SEG (0-15)
// 配置说明：SEG1-7接阳极（行），GRID1-5接阴极（列）
// 我们需要将7行映射到SEG，5列映射到GRID

// 行映射：SEG编号 (0-15) - 阳极行
#define SEG_ROW_1       0   // SEG1 - 第1行
#define SEG_ROW_2       1   // SEG2 - 第2行
#define SEG_ROW_3       2   // SEG3 - 第3行
#define SEG_ROW_4       3   // SEG4 - 第4行
#define SEG_ROW_5       4   // SEG5 - 第5行
#define SEG_ROW_6       5   // SEG6 - 第6行
#define SEG_ROW_7       6   // SEG7 - 第7行

// 列映射：GRID编号 (0-7) - 阴极列
#define GRID_COL_1      0   // GRID1 - 第1列
#define GRID_COL_2      1   // GRID2 - 第2列
#define GRID_COL_3      2   // GRID3 - 第3列
#define GRID_COL_4      3   // GRID4 - 第4列
#define GRID_COL_5      4   // GRID5 - 第5列
// GRID6-GRID8未使用

// ============================================================================
// 动画配置
// ============================================================================

// 动画帧间隔（毫秒）
#define ANIMATION_FRAME_INTERVAL_MS    110

// 开机动画持续时间（毫秒）
#define POWER_ON_ANIMATION_DURATION_MS 2000

// 关机动画持续时间（毫秒）
#define POWER_OFF_ANIMATION_DURATION_MS 800

// 滚动速度（像素/帧）
#define HORIZONTAL_SCROLL_SPEED        1
#define VERTICAL_SCROLL_SPEED          1

// UI切换动画持续时间（毫秒）
#define UI_TRANSITION_DURATION_MS      300

#endif /* __TM1640_CONFIG_H */
