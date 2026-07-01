#include "tm1640.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include "./SYSTEM/delay/delay.h"

// 全局配置
static TM1640_Config tm1640_config;

// 延迟函数（微秒级），需要根据实际系统时钟调整
static void tm1640_delay_us(uint32_t us) {
	  #if 0
    // 简单实现，实际项目应使用精确延时
    uint32_t count = us * (SystemCoreClock / 1000000) / 10;
    for (volatile uint32_t i = 0; i < count; i++);
	  #endif
	  delay_us(us);
}

// 初始化GPIO
void tm1640_gpio_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 使能时钟
    if (tm1640_config.clk_port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (tm1640_config.clk_port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (tm1640_config.clk_port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (tm1640_config.clk_port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();

    if (tm1640_config.din_port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (tm1640_config.din_port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (tm1640_config.din_port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (tm1640_config.din_port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();

    // 配置CLK引脚为推挽输出
    GPIO_InitStruct.Pin = tm1640_config.clk_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(tm1640_config.clk_port, &GPIO_InitStruct);

    // 配置DIN引脚为推挽输出
    GPIO_InitStruct.Pin = tm1640_config.din_pin;
    HAL_GPIO_Init(tm1640_config.din_port, &GPIO_InitStruct);

    // 初始状态：CLK高，DIN高
    HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_SET);
}

// 起始条件
void tm1640_start(void) {
	  HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_SET);
	  tm1640_delay_us(1);
	  HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_RESET);
	  tm1640_delay_us(1);
	  HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_RESET);
	  tm1640_delay_us(1);
}

// 停止条件
void tm1640_stop(void) {
	  HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_RESET);
	  HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_SET);  
	   tm1640_delay_us(1);
	  HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_SET);
	  tm1640_delay_us(1);
}

// 写一个字节（LSB first）
void tm1640_write_byte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        // 设置CLK低
        HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_RESET);
        //tm1640_delay_us(1);

        // 设置DIN为数据位（LSB first）
        if (data & 0x01) {
            HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(tm1640_config.din_port, tm1640_config.din_pin, GPIO_PIN_RESET);
        }
        tm1640_delay_us(10);

        // 上升沿锁存数据
        HAL_GPIO_WritePin(tm1640_config.clk_port, tm1640_config.clk_pin, GPIO_PIN_SET);
       // tm1640_delay_us(1);

        data >>= 1; // 移到下一位
    }
	tm1640_delay_us(10);
}

// 发送命令
void tm1640_send_command(uint8_t cmd) {
    tm1640_start();
    tm1640_write_byte(cmd);
    tm1640_stop();
}

// 发送数据到指定地址
void tm1640_send_data(uint8_t addr, uint8_t data) {
    tm1640_start();
    tm1640_write_byte(TM1640_CMD_DATA | 0x00);  // 固定地址模式，自动增加
    tm1640_stop();

    tm1640_start();
    tm1640_write_byte(TM1640_CMD_ADDR | (addr & 0x0F));  // 地址限制在0-15
    tm1640_write_byte(data);
    tm1640_stop();
}

// 发送多个数据
void tm1640_send_multiple_data(uint8_t start_addr, uint8_t *data, uint8_t len) {
    if (len == 0) return;

    tm1640_start();
    tm1640_write_byte(TM1640_CMD_DATA);   // 数据命令，自动地址增加
    tm1640_stop();

    tm1640_start();
    tm1640_write_byte(TM1640_CMD_ADDR | (start_addr & 0x0F));  // 地址命令，限制在0-15

    for (uint8_t i = 0; i < len; i++) {
        tm1640_write_byte(data[i]);
    }

    tm1640_stop();
}

// 初始化TM1640
void tm1640_init(TM1640_Config *config) {
    if (config == NULL) return;

    memcpy(&tm1640_config, config, sizeof(TM1640_Config));

    // 初始化GPIO
    tm1640_gpio_init();

    // 发送初始化命令：显示开，默认亮度
    tm1640_send_command(TM1640_CMD_DISPLAY | TM1640_DISPLAY_ON | tm1640_config.brightness);

    // 清除显示
    tm1640_clear_all();
}

// 设置亮度
void tm1640_set_brightness(uint8_t brightness) {
    if (brightness > 7) brightness = 7;
    tm1640_config.brightness = brightness;
    tm1640_send_command(TM1640_CMD_DISPLAY | TM1640_DISPLAY_ON | brightness);
}

// 开启显示
void tm1640_display_on(void) {
    tm1640_send_command(TM1640_CMD_DISPLAY | TM1640_DISPLAY_ON | tm1640_config.brightness);
}

// 关闭显示
void tm1640_display_off(void) {
    tm1640_send_command(TM1640_CMD_DISPLAY | TM1640_DISPLAY_OFF);
}

// 清除所有显示
void tm1640_clear_all(void) {
    uint8_t clear_data[16] = {0};
    tm1640_send_multiple_data(0, clear_data, 16);

} 
