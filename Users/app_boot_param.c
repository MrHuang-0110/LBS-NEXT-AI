#include "app_boot_param.h"
#include "./SYSTEM/delay/delay.h"
#include "usbd_conf.h"
#include "usbd_core.h"
#include "stm32f1xx_hal.h"

extern USBD_HandleTypeDef USBD_Device;

static void app_usb_disconnect_before_reset(void)
{
    (void)USBD_Stop(&USBD_Device);
    (void)USBD_DeInit(&USBD_Device);
    usbd_port_config(0);
    delay_ms(DRV_USB_DISCONNECT_MS);
}

void AppBoot_RequestFirmwareUpdate(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = APP_BOOT_PARAM_ADDR;
    erase.NbPages = 1U;
    if (HAL_FLASHEx_Erase(&erase, &page_error) == HAL_OK)
    {
        (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_BOOT_PARAM_ADDR, APP_BOOT_MAGIC);
        (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_BOOT_PARAM_ADDR + 4U, APP_BOOT_FLAG_UPDATE_REQ);
    }
    HAL_FLASH_Lock();
    app_usb_disconnect_before_reset();
    NVIC_SystemReset();
}
