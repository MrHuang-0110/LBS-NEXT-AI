#ifndef APP_BOOT_PARAM_H
#define APP_BOOT_PARAM_H

#include <stdint.h>

#define APP_BOOT_PARAM_ADDR         0x08007800U
#define APP_BOOT_MAGIC              0xB007E103U
#define APP_BOOT_FLAG_UPDATE_REQ    0x00000001U
#define DRV_USB_DISCONNECT_MS       500U

void AppBoot_RequestFirmwareUpdate(void);

#endif
