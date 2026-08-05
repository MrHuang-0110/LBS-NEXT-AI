#include "drv_flash_storage.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define COLOR_STORE_VERSION     1U

typedef struct
{
    uint8_t  valid;
    uint8_t  hub_id;
    uint16_t reserved;
    CALIBRATION cal;
    uint32_t crc32;
} FlashColorSlot_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t slot_count;
    FlashColorSlot_t slots[HUB_PORT_MAX_NUM];
    uint32_t bt_magic;
    uint8_t  bt_configured;
    uint8_t  bt_reserved[3];
} FlashColorStore_t;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
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

static uint32_t slot_crc(const CALIBRATION *cal)
{
    return crc32_update(0U, (const uint8_t *)cal, (uint32_t)sizeof(CALIBRATION));
}

static int flash_erase_page(uint32_t page_addr)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef st;

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages = 1U;
    st = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return (st == HAL_OK) ? 0 : -1;
}

static int flash_program_words(uint32_t addr, const uint32_t *src, uint32_t word_cnt)
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

static int flash_load_store(FlashColorStore_t *store)
{
    const FlashColorStore_t *flash_ptr =
        (const FlashColorStore_t *)DRV_FLASH_USER_BASE;
    FlashColorSlot_t saved_slots[HUB_PORT_MAX_NUM];
    uint32_t saved_bt_magic;
    uint8_t saved_bt_configured;

    memcpy(store, flash_ptr, sizeof(FlashColorStore_t));
    if (store->magic == DRV_FLASH_COLOR_MAGIC)
    {
        return 0;
    }

    memcpy(saved_slots, store->slots, sizeof(saved_slots));
    saved_bt_magic = store->bt_magic;
    saved_bt_configured = store->bt_configured;

    memset(store, 0, sizeof(FlashColorStore_t));
    store->magic = DRV_FLASH_COLOR_MAGIC;
    store->version = COLOR_STORE_VERSION;
    store->slot_count = HUB_PORT_MAX_NUM;
    memcpy(store->slots, saved_slots, sizeof(saved_slots));
    store->bt_magic = saved_bt_magic;
    store->bt_configured = saved_bt_configured;
    return 0;
}

static int flash_commit_store(const FlashColorStore_t *store)
{
    uint32_t words[(sizeof(FlashColorStore_t) + 3U) / 4U];

    memcpy(words, store, sizeof(FlashColorStore_t));
    if (flash_erase_page(DRV_FLASH_USER_BASE) != 0)
    {
        return -1;
    }
    return flash_program_words(DRV_FLASH_USER_BASE, words,
                               (uint32_t)((sizeof(FlashColorStore_t) + 3U) / 4U));
}

int DrvFlashStorage_Init(void)
{
    return 0;
}

int DrvFlashStorage_IsBtConfigured(void)
{
    FlashColorStore_t store;

    flash_load_store(&store);
    if ((store.bt_magic == DRV_FLASH_BT_MAGIC) && (store.bt_configured != 0U))
    {
        return 1;
    }
    return 0;
}

int DrvFlashStorage_SetBtConfigured(void)
{
    FlashColorStore_t store;

    flash_load_store(&store);
    store.magic = DRV_FLASH_COLOR_MAGIC;
    store.version = COLOR_STORE_VERSION;
    store.slot_count = HUB_PORT_MAX_NUM;
    store.bt_magic = DRV_FLASH_BT_MAGIC;
    store.bt_configured = 1U;
    return flash_commit_store(&store);
}

int DrvFlashStorage_ClearBtConfigured(void)
{
    FlashColorStore_t store;

    flash_load_store(&store);
    store.bt_magic = 0U;
    store.bt_configured = 0U;
    return flash_commit_store(&store);
}

int DrvFlashStorage_ReadColorCalib(uint8_t hub_id, CALIBRATION *out)
{
    FlashColorStore_t store;
    uint8_t i;

    if ((out == NULL) || (hub_id >= HUB_PORT_MAX_NUM))
    {
        return -1;
    }

    flash_load_store(&store);
    memset(out, 0, sizeof(CALIBRATION));

    for (i = 0U; i < HUB_PORT_MAX_NUM; i++)
    {
        const FlashColorSlot_t *slot = &store.slots[i];
        if ((slot->valid != 0U) && (slot->hub_id == hub_id))
        {
            if (slot->crc32 == slot_crc(&slot->cal))
            {
                memcpy(out, &slot->cal, sizeof(CALIBRATION));
                return 0;
            }
        }
    }
    return -1;
}

int DrvFlashStorage_WriteColorCalib(uint8_t hub_id, const CALIBRATION *in)
{
    FlashColorStore_t store;
    uint8_t i;
    uint8_t target = 0xFFU;

    if ((in == NULL) || (hub_id >= HUB_PORT_MAX_NUM))
    {
        return -1;
    }

    flash_load_store(&store);
    store.magic = DRV_FLASH_COLOR_MAGIC;
    store.version = COLOR_STORE_VERSION;
    store.slot_count = HUB_PORT_MAX_NUM;

    for (i = 0U; i < HUB_PORT_MAX_NUM; i++)
    {
        if ((store.slots[i].valid != 0U) && (store.slots[i].hub_id == hub_id))
        {
            target = i;
            break;
        }
        if ((target == 0xFFU) && (store.slots[i].valid == 0U))
        {
            target = i;
        }
    }
    if (target == 0xFFU)
    {
        target = hub_id;
    }

    store.slots[target].valid = 1U;
    store.slots[target].hub_id = hub_id;
    store.slots[target].reserved = 0U;
    memcpy(&store.slots[target].cal, in, sizeof(CALIBRATION));
    store.slots[target].crc32 = slot_crc(in);

    return flash_commit_store(&store);
}
