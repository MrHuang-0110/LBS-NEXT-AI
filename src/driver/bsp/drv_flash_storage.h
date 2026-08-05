#ifndef DRV_FLASH_STORAGE_H
#define DRV_FLASH_STORAGE_H

#include <stdint.h>
#include "color.h"

/* 用户数据区：链接脚本 IROM 末端 0x08078000，512KB Flash 最后 2KB 页 */
#define DRV_FLASH_USER_BASE     0x08078000U
#define DRV_FLASH_USER_SIZE     0x00000800U
#define DRV_FLASH_PAGE_SIZE     0x00000800U
#define HUB_PORT_MAX_NUM        2U

#define DRV_FLASH_COLOR_MAGIC   0x504B434CU  /* 'PKCL' */
#define DRV_FLASH_BT_MAGIC      0x42454332U  /* 'ECB2' */

int DrvFlashStorage_Init(void);
int DrvFlashStorage_IsBtConfigured(void);
int DrvFlashStorage_SetBtConfigured(void);
int DrvFlashStorage_ClearBtConfigured(void);
int DrvFlashStorage_ReadColorCalib(uint8_t hub_id, CALIBRATION *out);
int DrvFlashStorage_WriteColorCalib(uint8_t hub_id, const CALIBRATION *in);

#endif
