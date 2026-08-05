#ifndef APP_PIKA_SCRIPT_FLASH_H
#define APP_PIKA_SCRIPT_FLASH_H

#include <stdint.h>

/* 脚本持久化区: Flash 0x08060000 ~ 0x08067FFF, 32KB (避开 APP 代码) */
#define APP_SCRIPT_FLASH_ADDR       0x08060000U
#define APP_SCRIPT_FLASH_SIZE       (32U * 1024U)
#define APP_SCRIPT_FLASH_MAGIC      0x6F795053U  /* 'SPyo' */
#define APP_SCRIPT_FLASH_VERSION    1U
#define APP_SCRIPT_FLASH_HEADER_SIZE 16U
#define APP_SCRIPT_FLASH_MAX_PAYLOAD (APP_SCRIPT_FLASH_SIZE - APP_SCRIPT_FLASH_HEADER_SIZE - 4U)

/* 启动时检测 Flash 脚本区, 有效则拷贝到 RAM 并装载; 失败返回负值 */
int AppPikaScriptFlash_LoadToRam(void);

/* 关机时把当前 RAM 脚本保存到 Flash; 失败返回负值 */
int AppPikaScriptFlash_SaveFromRam(void);

/* 快速检查 Flash 脚本区是否有有效脚本, 有效返回 1, 无效返回 0 */
int AppPikaScriptFlash_HasValid(void);

/* 擦除 Flash 脚本区, 标记为无效 */
int AppPikaScriptFlash_Invalidate(void);

#endif