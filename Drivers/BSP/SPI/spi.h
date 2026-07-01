#ifndef __SPI_H
#define __SPI_H

#include "./SYSTEM/sys/sys.h"


#define SPI2_SCK_GPIO_PORT              GPIOB
#define SPI2_SCK_GPIO_PIN               GPIO_PIN_13
#define SPI2_SCK_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   

#define SPI2_MISO_GPIO_PORT             GPIOB
#define SPI2_MISO_GPIO_PIN              GPIO_PIN_14
#define SPI2_MISO_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)   

#define SPI2_MOSI_GPIO_PORT             GPIOB
#define SPI2_MOSI_GPIO_PIN              GPIO_PIN_15
#define SPI2_MOSI_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)    

#define SPI2_CS_GPIO_PORT               GPIOB
#define SPI2_CS_GPIO_PIN                GPIO_PIN_12
#define SPI2_CS_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)    

 
#define SPI2_SPI                        SPI2
#define SPI2_SPI_CLK_ENABLE()           do{ __HAL_RCC_SPI2_CLK_ENABLE(); }while(0)    /* SPI1?????? */


 
#define SPI_SPEED_2         0
#define SPI_SPEED_4         1
#define SPI_SPEED_8         2
#define SPI_SPEED_16        3
#define SPI_SPEED_32        4
#define SPI_SPEED_64        5
#define SPI_SPEED_128       6
#define SPI_SPEED_256       7


void spi2_init(void);
void spi2_set_speed(uint8_t speed);
uint8_t spi2_read_write_byte(uint8_t txdata);
void spi2_write_more_byte(uint8_t *data,uint16_t length);
void spi2_receive_more_byte(uint8_t *data,uint16_t lengt);
void spi2_tx_rx_more_byte(uint8_t *txdata,uint8_t *rxdata,uint16_t length);



#endif
