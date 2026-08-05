#include "blue.h"
#include "drv_flash_storage.h"
#include "drv_bt_config.h"
#include "drv_comm.h"
#include "event_manager.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include "string.h"
#include "malloc.h"
#include "protocol.h"
#include "usart.h"
#include "delay.h"

static uint8_t blue_uart_send(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t len)
{
    uint32_t t0;

    if ((huart == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    t0 = HAL_GetTick();
    while (HAL_UART_GetState(huart) != HAL_UART_STATE_READY)
    {
        if ((HAL_GetTick() - t0) >= 200U)
        {
            return 0U;
        }
        delay_ms(1U);
    }
    return (HAL_UART_Transmit(huart, (uint8_t *)data, len, 500U) == HAL_OK) ? 1U : 0U;
}

static uint8_t blue_resp_has_ok(const char *resp)
{
    if (resp == NULL)
    {
        return 0U;
    }
    if ((strncmp(resp, "OK\r\n", 4) == 0) || (strcmp(resp, "OK") == 0))
    {
        return 1U;
    }
    return (strstr(resp, "\r\nOK\r\n") != NULL) ? 1U : 0U;
}

static uint8_t blue_at_cmd(char *atcmd)
{
    uint8_t retry;
    DEV_BLUE *blue = (DEV_BLUE *)getHubBase(PORT_BLUE);

    if ((blue == NULL) || (blue->huart == NULL))
    {
        return 0U;
    }

    for (retry = 0U; retry < 3U; retry++)
    {
        blue->is_resh_flag = false;
        DrvBtRing_Flush();
        if (blue_uart_send(blue->huart, (const uint8_t *)atcmd, (uint16_t)strlen(atcmd)) == 0U)
        {
            delay_ms(DRV_BT_AT_GAP_MS);
            continue;
        }
        delay_ms(DRV_BT_AT_TIMEOUT_MS);
        if ((blue->is_resh_flag != 0U) &&
            (blue_resp_has_ok((const char *)blue->at_cmd_bufer) != 0U))
        {
            return 1U;
        }
        delay_ms(DRV_BT_AT_GAP_MS);
    }
    return 0U;
}

static uint8_t blue_run_first_time_config(void)
{
    char cmd[48];
    uint8_t retry;

    /* 不做 AT+FACTORY/AT+RST，避免覆盖用户已设置的蓝牙参数 */
    if (blue_at_cmd("AT\r\n") == 0U)
    {
        return 0U;
    }
    delay_ms(DRV_BT_AT_GAP_MS);

    (void)snprintf(cmd, sizeof(cmd), "AT+NAME=%s\r\n", DRV_BT_NAME);
    if (blue_at_cmd(cmd) == 0U)
    {
        return 0U;
    }
    delay_ms(DRV_BT_AT_GAP_MS);

    if (blue_at_cmd("AT+ROLE=2\r\n") == 0U)
    {
        return 0U;
    }
    delay_ms(DRV_BT_AT_GAP_MS);

    for (retry = 0U; retry < 3U; retry++)
    {
        if (DrvFlashStorage_SetBtConfigured() == 0)
        {
            if (DrvFlashStorage_IsBtConfigured() != 0)
            {
                return 1U;
            }
        }
        delay_ms(20U);
    }
    return 0U;
}

void blue_set_on(void)
{
    (void)blue_at_cmd("AT+ROLE=2\r\n");
}

void blue_set_off(void)
{
    (void)blue_at_cmd("AT+ROLE=1\r\n");
}

void blue_send_data(char *str, uint16_t len)
{
    DEV_BLUE *blue = (DEV_BLUE *)getHubBase(PORT_BLUE);
    if (blue != NULL)
    {
        uart_transmit_it(blue->huart, (uint8_t *)str, len);
    }
}

void blue_init(void)
{
    DEV_BLUE *blue;

    identify_and_bind(&hub_port[PORT_BLUE], DEVICE_BLUE_ID);
    blue = (DEV_BLUE *)getHubBase(PORT_BLUE);
    if (blue == NULL)
    {
        return;
    }

    blue->cfg.blue_init_state = 0;
    blue->cfg.on_off = 0;
    blue->is_off_on = 0;

    /*
     * Flash 标记仅表示「MCU 已做过一次性蓝牙初始化」。
     * 有标记则开机不再向模块发任何 AT；之后名字/广播由模块自己保存，
     * 用户通过 USB 手动发 AT 时才会改，MCU 不会主动再管。
     */
    if (DrvFlashStorage_IsBtConfigured() != 0)
    {
        blue->cfg.blue_init_state = 1;
        return;
    }

    if (blue_run_first_time_config() != 0U)
    {
        blue->cfg.blue_init_state = 1;
    }
}

void blue_force_reinit(void)
{
    (void)DrvFlashStorage_ClearBtConfigured();
    blue_init();
}

void refsh_blue(void *self, void *data)
{
    DEV_BLUE *mt = (DEV_BLUE *)self;
    _AGREEMENT *_fd = (_AGREEMENT *)data;
    switch (_fd->index)
    {
        case 0xC1:
            memcpy(mt->remoteValue, _fd->data, 10);
            break;
        default:
            break;
    }
}

DEV_BLUE *create_blue(void)
{
    DEV_BLUE *blue = mymalloc(SRAMIN, sizeof(DEV_BLUE));
    if (blue == NULL)
    {
        return NULL;
    }
    memset(blue, 0, sizeof(DEV_BLUE));
    blue->base.type = DEVICE_BLUE_ID;
    memset(blue->base.name, 0, sizeof(blue->base.name));
    strcpy(blue->base.name, "blue");
    blue->huart = getusartHandle(5);
    return blue;
}

DEV_BLUE *read_blue(void *self)
{
    return (DEV_BLUE *)self;
}
