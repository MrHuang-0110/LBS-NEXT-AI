#ifndef __MATRIX_PORT_H
#define __MATRIX_PORT_H

#include <stdint.h>
#include <stdbool.h>

// 矩阵尺寸定义（与tm1640_config.h一致）
#define MATRIX_PORT_ROWS 7
#define MATRIX_PORT_COLS 5

// 退出检查函数指针类型
// 返回true表示需要退出滚动，false表示继续滚动
typedef bool (*matrix_port_exit_check_t)(void);

/**
 * @brief 显示图案
 * @param pattern 7个字节的数组，每个字节表示一行的列数据
 *               bit0对应第0列，bit1对应第1列，... bit4对应第4列
 *               高位bit5-7忽略
 *               调用后立即刷新显示
 *               注意：x代表行(0-6)，y代表列(0-4)
 */
void matrix_port_display_pattern(const uint8_t pattern[7]);

/**
 * @brief 滚动显示字符串（横屏滚动，阻塞式）
 * @param str 要显示的字符串指针
 *            如果字符串长度为1，则静态显示不滚动
 *            如果字符串长度>1，则水平向左滚动直到文本完全显示
 *            函数内部阻塞直到滚动完成
 * @param exit_check 退出检查函数指针，可设为NULL
 *                   当函数返回true时退出滚动，返回false时继续滚动
 */
void matrix_port_scroll_text(const char *str, matrix_port_exit_check_t exit_check);

/**
 * @brief 设置矩阵灯整体亮度
 * @param brightness 亮度值 0-7，0最暗，7最亮
 */
void matrix_port_set_brightness(uint8_t brightness);

/**
 * @brief 点亮指定LED灯
 * @param x 行坐标 0-6（0最上，6最下）
 * @param y 列坐标 0-4（0最左，4最右）
 *           调用后立即刷新显示
 */
void matrix_port_set_pixel(uint8_t x, uint8_t y);

/**
 * @brief 设置指定LED灯的亮度
 * @note TM1640不支持单个LED亮度控制，此函数仅控制LED开关
 * @param x 行坐标 0-6
 * @param y 列坐标 0-4
 * @param b 开关值 0熄灭，>0点亮
 */
void matrix_port_set_pixel_brightness(uint8_t x, uint8_t y, uint8_t b);

/**
 * @brief 清屏
 *        熄灭所有LED，立即刷新显示
 */
void matrix_port_clear(void);


/**
 * @brief 初始化矩阵端口
 *        调用其他函数前应先初始化
 */
void matrix_port_init(void);

bool uvm_exit(void);
#endif /* __MATRIX_PORT_H */