#ifndef __TM1640_H
#define __TM1640_H

#include "stm32f1xx_hal.h"  // 根据实际项目调整
#include <stdint.h>

// TM1640配置结构体
typedef struct {
    GPIO_TypeDef *clk_port;
    uint16_t clk_pin;
    GPIO_TypeDef *din_port;
    uint16_t din_pin;
    uint8_t brightness;  // 亮度 0-7
} TM1640_Config;

// TM1640命令定义
#define TM1640_CMD_DATA     0x40  // 数据命令设置
#define TM1640_CMD_ADDR     0xC0  // 地址命令设置
#define TM1640_CMD_DISPLAY  0x80  // 显示控制命令

// 显示开关
#define TM1640_DISPLAY_OFF  0x00
#define TM1640_DISPLAY_ON   0x08

// 亮度级别 (0-7)
#define TM1640_BRIGHTNESS_0 0x00
#define TM1640_BRIGHTNESS_1 0x01
#define TM1640_BRIGHTNESS_2 0x02
#define TM1640_BRIGHTNESS_3 0x03
#define TM1640_BRIGHTNESS_4 0x04
#define TM1640_BRIGHTNESS_5 0x05
#define TM1640_BRIGHTNESS_6 0x06
#define TM1640_BRIGHTNESS_7 0x07

// 初始化TM1640
void tm1640_init(TM1640_Config *config);

// 发送命令
void tm1640_send_command(uint8_t cmd);

// 发送数据到指定地址
void tm1640_send_data(uint8_t addr, uint8_t data);

// 发送多个数据
void tm1640_send_multiple_data(uint8_t start_addr, uint8_t *data, uint8_t len);

// 设置亮度
void tm1640_set_brightness(uint8_t brightness);

// 开启/关闭显示
void tm1640_display_on(void);
void tm1640_display_off(void);

// 清除所有显示
void tm1640_clear_all(void);

// 底层GPIO操作（内部使用，外部也可用）
void tm1640_gpio_init(void);
void tm1640_start(void);
void tm1640_stop(void);
void tm1640_write_byte(uint8_t data);

#endif /* __TM1640_H */
