#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

// 字体尺寸
#define FONT_WIDTH       5
#define FONT_HEIGHT      7

// ASCII字符范围
#define FONT_FIRST_CHAR  32   // 空格
#define FONT_LAST_CHAR   126  // ~
#define FONT_CHAR_COUNT  (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

// 字符数据格式：5列，每列一个字节，每个字节的位0-6表示行（0=底部，6=顶部？）
// 我们定义为：位0 = 第0行（顶部），位6 = 第6行（底部）或反过来
// 根据实际显示方向调整

// 获取字符数据指针
const uint8_t *font_get_char_data(char c);

// 字符宽度（固定为5）
uint8_t font_get_char_width(char c);

// 字符高度（固定为7）
uint8_t font_get_char_height(void);

// 字符串宽度（像素）
uint8_t font_get_string_width(const char *str);

// 绘制字符到缓冲区
void font_draw_char(char c, uint8_t x, uint8_t y, uint8_t *buffer, uint8_t buffer_width, uint8_t buffer_height);

// 绘制字符串到缓冲区
void font_draw_string(const char *str, uint8_t x, uint8_t y, uint8_t *buffer, uint8_t buffer_width, uint8_t buffer_height);

#endif /* __FONT_H */
