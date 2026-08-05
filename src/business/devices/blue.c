#include "blue.h"
#include "at.h"
#include "drv_flash_storage.h"
#include "drv_bt_config.h"
#include "drv_comm.h"
#include "event_manager.h"
#include <stdio.h>
#include "string.h"
#include "malloc.h"
#include "frame.h"
#include "usart.h"
#include "delay.h"

/* AT 细节（重试/OK 判定/缓存更新）全部在 protocol/at.c，本文件不再维护第二套实现（Task 17）*/

static uint8_t blue_run_first_time_config(void)
{
    char cmd[48];
    uint8_t retry;
    uint8_t ok;

    /* 不做 AT+FACTORY/AT+RST，避免覆盖用户已设置的蓝牙参数 */
    if ((At_Exchange("AT", &ok) == 0U) || (ok == 0U))
    {
        return 0U;
    }
    delay_ms(DRV_BT_AT_GAP_MS);

    (void)snprintf(cmd, sizeof(cmd), "AT+NAME=%s", DRV_BT_NAME);
    if ((At_Exchange(cmd, &ok) == 0U) || (ok == 0U))
    {
        return 0U;
    }
    delay_ms(DRV_BT_AT_GAP_MS);

    if ((At_Exchange("AT+ROLE=2", &ok) == 0U) || (ok == 0U))
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
    uint8_t ok;
    (void)At_Exchange("AT+ROLE=2", &ok);
}

void blue_set_off(void)
{
    uint8_t ok;
    (void)At_Exchange("AT+ROLE=1", &ok);
}

void blue_send_data(char *str, uint16_t len)
{
    (void)DrvComm_BtSendData((const uint8_t *)str, len);
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

SensorBase *create_blue(void)
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
    return (SensorBase *)blue;
}

void destroy_blue(SensorBase *sensor)
{
    DEV_BLUE *blue = (DEV_BLUE *)sensor;
    myfree(SRAMIN, blue);
}

DEV_BLUE *read_blue(void *self)
{
    return (DEV_BLUE *)self;
}
