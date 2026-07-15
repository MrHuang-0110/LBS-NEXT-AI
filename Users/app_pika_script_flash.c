#include "app_pika_script_flash.h"
#include "app_pika_runtime.h"
#include "drv_mem.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* 脚本区: 32KB = 16 个 2KB 页 */
#define SCRIPT_FLASH_PAGE_SIZE      0x800U
#define SCRIPT_FLASH_PAGE_COUNT     16U

/* ------------------------------------------------------------------ */
/* Flash 底层操作 (复用 HAL, 与 drv_flash_storage.c 风格一致)         */
/* ------------------------------------------------------------------ */

static int script_flash_erase_pages(uint32_t start_addr, uint32_t page_count)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef st;

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = start_addr;
    erase.NbPages = page_count;
    st = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return (st == HAL_OK) ? 0 : -1;
}

static int script_flash_program_words(uint32_t addr, const uint32_t *src, uint32_t word_cnt)
{
    uint32_t i;
    HAL_StatusTypeDef st;

    HAL_FLASH_Unlock();
    for (i = 0U; i < word_cnt; i++)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * 4U), src[i]);
        if (st != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/* ------------------------------------------------------------------ */
/* CRC32 复用 drv_flash_storage.c 的 crc32_update 算法                */
/* ------------------------------------------------------------------ */

static uint32_t script_flash_crc32(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    crc = ~crc;
    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

/* ------------------------------------------------------------------ */
/* 对外 API                                                           */
/* ------------------------------------------------------------------ */

int AppPikaScriptFlash_HasValid(void)
{
    const uint32_t *flash = (const uint32_t *)APP_SCRIPT_FLASH_ADDR;
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    uint32_t total_len;
    uint32_t pad;
    uint32_t crc_stored;
    uint32_t crc_calc;

    magic = flash[0];
    if (magic != APP_SCRIPT_FLASH_MAGIC)
    {
        return 0;
    }

    version = flash[1];
    if (version != APP_SCRIPT_FLASH_VERSION)
    {
        return 0;
    }

    length = flash[2];
    if (length == 0U || length > APP_SCRIPT_FLASH_MAX_PAYLOAD)
    {
        return 0;
    }

    /* CRC32 覆盖: header + script_data + padding */
    total_len = APP_SCRIPT_FLASH_HEADER_SIZE + length;
    pad = (4U - (total_len % 4U)) % 4U;
    total_len += pad;

    crc_stored = *(const uint32_t *)(APP_SCRIPT_FLASH_ADDR + total_len);
    crc_calc = script_flash_crc32(0U, (const uint8_t *)APP_SCRIPT_FLASH_ADDR, total_len);

    return (crc_stored == crc_calc) ? 1 : 0;
}

int AppPikaScriptFlash_LoadToRam(void)
{
    const uint32_t *flash = (const uint32_t *)APP_SCRIPT_FLASH_ADDR;
    uint32_t length;
    const uint8_t *script_data;

    if (AppPikaScriptFlash_HasValid() == 0)
    {
        return -1;
    }

    length = flash[2];
    script_data = (const uint8_t *)(APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE);

    /* 拷贝到 RAM 缓冲 */
    if (length > DrvMem_GetFileBufferSize())
    {
        return -2;
    }
    memcpy(DrvMem_GetFileBuffer(), script_data, length);
    DrvMem_SetFileLength(length);

    /* 装载到 PikaVM; 返回 READY 状态, 不自动运行 */
    return AppPika_LoadBytecode(DrvMem_GetFileBuffer(), length);
}

int AppPikaScriptFlash_SaveFromRam(void)
{
    uint32_t length;
    const uint8_t *ram;
    uint32_t header[4];
    uint32_t crc;
    uint32_t i;
    uint32_t word_cnt;
    uint32_t total_data_len;
    uint32_t pad;
    uint32_t w;
    uint32_t off;
    uint32_t remain;
    uint32_t j;

    length = DrvMem_GetFileLength();
    ram = DrvMem_GetFileBuffer();

    if (length == 0U || ram == NULL)
    {
        return -1;
    }
    if (length > APP_SCRIPT_FLASH_MAX_PAYLOAD)
    {
        return -1;
    }

    /* 1. 擦除整个脚本区 */
    if (script_flash_erase_pages(APP_SCRIPT_FLASH_ADDR, SCRIPT_FLASH_PAGE_COUNT) != 0)
    {
        return -2;
    }

    /* 2. 写 header (4 个 word) */
    header[0] = APP_SCRIPT_FLASH_MAGIC;
    header[1] = APP_SCRIPT_FLASH_VERSION;
    header[2] = length;
    header[3] = 0U;
    if (script_flash_program_words(APP_SCRIPT_FLASH_ADDR, header, 4U) != 0)
    {
        return -3;
    }

    /* 3. 写 script_data, word 对齐, 不足 4 字节用 0x1A 填充 */
    word_cnt = (length + 3U) / 4U;
    for (i = 0U; i < word_cnt; i++)
    {
        off = i * 4U;
        remain = length - off;
        if (remain >= 4U)
        {
            w = ((uint32_t)ram[off])
              | ((uint32_t)ram[off + 1U] << 8U)
              | ((uint32_t)ram[off + 2U] << 16U)
              | ((uint32_t)ram[off + 3U] << 24U);
        }
        else
        {
            w = 0U;
            for (j = 0U; j < remain; j++)
            {
                ((uint8_t *)&w)[j] = ram[off + j];
            }
            for (j = remain; j < 4U; j++)
            {
                ((uint8_t *)&w)[j] = 0x1AU;
            }
        }
        if (script_flash_program_words(APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE + off, &w, 1U) != 0)
        {
            return -4;
        }
    }

    /* 4. 计算 CRC32: header + script_data + padding */
    total_data_len = APP_SCRIPT_FLASH_HEADER_SIZE + length;
    pad = (4U - (total_data_len % 4U)) % 4U;
    crc = script_flash_crc32(0U, (const uint8_t *)header, sizeof(header));
    crc = script_flash_crc32(crc, ram, length);
    if (pad > 0U)
    {
        static const uint8_t pad_bytes[3] = {0x1AU, 0x1AU, 0x1AU};
        crc = script_flash_crc32(crc, pad_bytes, pad);
    }

    /* 5. 写 CRC32 */
    return script_flash_program_words(
        APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE + word_cnt * 4U, &crc, 1U);
}

int AppPikaScriptFlash_Invalidate(void)
{
    return script_flash_erase_pages(APP_SCRIPT_FLASH_ADDR, 1U);
}