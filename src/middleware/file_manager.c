/**
 * @file file_manager.c
 * @brief STM32F103RCT6小型文件系统实现
 */

#include "file_manager.h"
#include "malloc.h"
#include <string.h>

/*=============================================================================
 *                          内部宏定义和常量
 *============================================================================*/

/* FAT表相关 */
#define FM_FAT_MAGIC                 0x46544D46U  /* "FMTF" */
#define FM_FAT_VERSION               0x00010000U  /* v1.0 */

/* FAT表项大小 */
#define FM_FAT_ENTRY_SIZE            sizeof(fm_fat_entry_t)

/* 计算FAT表最大条目数 */
#define FM_FAT_MAX_ENTRIES           (FM_FAT_SIZE / FM_FAT_ENTRY_SIZE)

/* 系统区最大扇区数 */
#define FM_SYSTEM_MAX_SECTORS        (FM_SYSTEM_ARRAY_SIZE / FM_FLASH_SECTOR_SIZE)

/* 用户区最大扇区数 */
#define FM_USER_MAX_SECTORS          (FM_USER_ARRAY_SIZE / FM_FLASH_SECTOR_SIZE)

/* 系统区文件占用的扇区数（固定1个扇区） */
#define FM_SYSTEM_FILE_SECTORS       1U

/* 检查地址是否对齐到扇区 */
#define FM_IS_SECTOR_ALIGNED(addr)   (((addr) & (FM_FLASH_SECTOR_SIZE - 1)) == 0)

/* 计算扇区索引 */
#define FM_ADDR_TO_SECTOR_INDEX(addr, base) \
    (((addr) - (base)) / FM_FLASH_SECTOR_SIZE)

/* 计算扇区起始地址 */
#define FM_SECTOR_TO_ADDR(base, index) \
    ((base) + (index) * FM_FLASH_SECTOR_SIZE)

/*=============================================================================
 *                          内部数据结构
 *============================================================================*/

/**
 * @brief FAT表项结构
 */
typedef struct {
    char filename[FM_FILENAME_MAX_LEN];  /* 文件名（以'\\0'结束） */
    fm_file_type_t type;                 /* 文件类型 */
    fm_file_status_t status;             /* 文件状态 */
    uint32_t start_sector;               /* 起始扇区索引 */
    uint32_t sector_count;               /* 占用扇区数 */
    uint32_t file_size;                  /* 文件实际大小（字节） */
    uint16_t crc16;                      /* CRC16校验值 */
    uint32_t timestamp;                  /* 时间戳 */
    uint8_t reserved[2];                 /* 保留字段，用于对齐（从10减到2字节） */
} __attribute__((packed)) fm_fat_entry_t;

/**
 * @brief FAT表头结构
 */
typedef struct {
    uint32_t magic;             /* 魔数："FMTF" */
    uint32_t version;           /* 版本号 */
    uint32_t entry_count;       /* 有效条目数 */
    uint32_t system_used;       /* 系统区已使用扇区数 */
    uint32_t user_used;         /* 用户区已使用扇区数 */
    uint32_t checksum;          /* 头部校验和 */
    uint8_t reserved[40];       /* 保留字段 */
} __attribute__((packed)) fm_fat_header_t;

/**
 * @brief 文件系统内部状态
 */
typedef struct {
    bool initialized;           /* 是否已初始化 */
    fm_fat_header_t fat_header; /* FAT表头 */
    fm_fat_entry_t fat_entries[FM_FAT_MAX_ENTRIES]; /* FAT表项数组 */
    bool sector_bitmap_system[FM_SYSTEM_MAX_SECTORS]; /* 系统区扇区位图 */
    bool sector_bitmap_user[FM_USER_MAX_SECTORS];   /* 用户区扇区位图 */
} fm_state_t;

/*=============================================================================
 *                          全局变量
 *============================================================================*/

static fm_state_t g_fm_state = {0};

/*=============================================================================
 *                          内部函数声明
 *============================================================================*/

static bool fm_load_fat(void);
static bool fm_save_fat(void);
static bool fm_update_sector_bitmap(void);
static uint32_t fm_find_free_sectors(fm_file_type_t type, uint32_t sector_count);
static bool fm_allocate_sectors(fm_file_type_t type, uint32_t start_sector, uint32_t sector_count);
static bool fm_free_sectors(fm_file_type_t type, uint32_t start_sector, uint32_t sector_count);
static fm_fat_entry_t* fm_find_fat_entry(const char *filename, fm_file_type_t type);
static bool fm_validate_file(const fm_fat_entry_t *entry);
static uint32_t fm_calculate_required_sectors(uint32_t file_size);
static bool fm_is_valid_filename_char(char c);

/*=============================================================================
 *                          文件系统API实现
 *============================================================================*/

/**
 * @brief 初始化文件系统
 */
bool fm_init(void)
{
    /* 已经初始化过 */
    if (g_fm_state.initialized) {
        return true;
    }

    /* 加载FAT表 */
    if (!fm_load_fat()) {
        /* FAT表无效，格式化系统 */
				fm_flash_erase_sector(FM_FAT_BASE_ADDR);
        return false;
    }

    /* 更新扇区位图 */
    if (!fm_update_sector_bitmap()) {
        return false;
    }

    g_fm_state.initialized = true;
    return true;
}

/**
 * @brief 格式化指定区域
 */
bool fm_format(fm_file_type_t type)
{
    if (!g_fm_state.initialized) {
        return false;
    }

    /* 擦除对应区域的扇区 */
    uint32_t base_addr = (type == FM_FILE_TYPE_SYSTEM) ?
                         FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;
    uint32_t sector_count = (type == FM_FILE_TYPE_SYSTEM) ?
                           FM_SYSTEM_MAX_SECTORS : FM_USER_MAX_SECTORS;

    for (uint32_t i = 0; i < sector_count; i++) {
        uint32_t sector_addr = FM_SECTOR_TO_ADDR(base_addr, i);
        if (!fm_flash_erase_sector(sector_addr)) {
            return false;
        }
    }

    /* 从FAT中删除对应类型的文件 */
    uint32_t removed_count = 0;
    for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
        fm_fat_entry_t *entry = &g_fm_state.fat_entries[i];
        if (entry->type == type && entry->status == FM_FILE_STATUS_USED) {
            entry->status = FM_FILE_STATUS_FREE;
            removed_count++;
        }
    }

    /* 更新FAT表头 */
    if (type == FM_FILE_TYPE_SYSTEM) {
        g_fm_state.fat_header.system_used = 0;
    } else {
        g_fm_state.fat_header.user_used = 0;
    }

    /* 保存FAT表 */
    if (!fm_save_fat()) {
        return false;
    }

    /* 更新扇区位图 */
    if (!fm_update_sector_bitmap()) {
        return false;
    }

    return true;
}

/**
 * @brief 打开文件
 */
bool fm_open(const char *filename, fm_file_type_t type, fm_file_handle_t *handle)
{
    if (!handle || !g_fm_state.initialized || !filename) {
        return false;
    }

    /* 查找FAT表项 */
    fm_fat_entry_t *entry = fm_find_fat_entry(filename, type);
    if (!entry || entry->status != FM_FILE_STATUS_USED) {
        return false;
    }

    /* 验证文件完整性 */
    if (!fm_validate_file(entry)) {
        entry->status = FM_FILE_STATUS_CORRUPT;
        fm_save_fat();
        return false;
    }

    /* 填充句柄 */
    memset(handle, 0, sizeof(fm_file_handle_t));
    strncpy(handle->info.filename, entry->filename, FM_FILENAME_MAX_LEN - 1);
    handle->info.filename[FM_FILENAME_MAX_LEN - 1] = '\0';
    handle->info.type = type;
    handle->info.status = FM_FILE_STATUS_USED;
    handle->info.start_addr = FM_SECTOR_TO_ADDR(
        (type == FM_FILE_TYPE_SYSTEM) ? FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR,
        entry->start_sector
    );
    handle->info.size = entry->file_size;
    handle->info.sectors = entry->sector_count;
    handle->info.crc16 = entry->crc16;
    handle->info.timestamp = entry->timestamp;
    handle->current_pos = 0;
    handle->is_open = true;

    return true;
}

/**
 * @brief 关闭文件
 */
bool fm_close(fm_file_handle_t *handle)
{
    if (!handle || !handle->is_open) {
        return false;
    }

    handle->is_open = false;
    return true;
}

/**
 * @brief 创建/写入文件
 */
bool fm_write(const char *filename, fm_file_type_t type, const uint8_t *data, uint32_t size)
{
    if (!data || size == 0 || !g_fm_state.initialized || !filename) {
        return false;
    }

    /* 验证文件名 */
    if (!fm_validate_filename(filename)) {
        return false;
    }

    /* 检查文件大小限制 */
    if (type == FM_FILE_TYPE_SYSTEM) {
        if (size > FM_SYSTEM_FILE_SIZE) {
            return false;
        }
    } else {
        if (size > FM_USER_MAX_FILE_SIZE) {
            return false;
        }
    }

    /* 计算需要的扇区数 */
    uint32_t required_sectors = fm_calculate_required_sectors(size);

    /* 查找是否已存在同名文件 */
    fm_fat_entry_t *existing_entry = fm_find_fat_entry(filename, type);
    if (existing_entry && existing_entry->status == FM_FILE_STATUS_USED) {
        /* 文件已存在，擦除原有扇区 */
        uint32_t base_addr = (type == FM_FILE_TYPE_SYSTEM) ?
                            FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;
        uint32_t start_addr = FM_SECTOR_TO_ADDR(base_addr, existing_entry->start_sector);

        /* 擦除所有占用的扇区 */
        for (uint32_t i = 0; i < existing_entry->sector_count; i++) {
            if (!fm_flash_erase_sector(start_addr + i * FM_FLASH_SECTOR_SIZE)) {
                return false;
            }
        }
			if (type == FM_FILE_TYPE_SYSTEM) {
          g_fm_state.fat_header.system_used -= existing_entry->sector_count;
      } else {
          g_fm_state.fat_header.user_used -= existing_entry->sector_count;
      }
        /* 释放扇区 */
        fm_free_sectors(type, existing_entry->start_sector, existing_entry->sector_count);

        /* 标记为未使用 */
        existing_entry->status = FM_FILE_STATUS_FREE;
    }

    /* 查找空闲扇区 */
    uint32_t start_sector = fm_find_free_sectors(type, required_sectors);
    if (start_sector == 0xFFFFFFFF) {
        return false; /* 空间不足 */
    }

    /* 分配扇区 */
    if (!fm_allocate_sectors(type, start_sector, required_sectors)) {
        return false;
    }

    /* 计算CRC16 */
    uint16_t crc = fm_crc16(data, size);

    /* 写入文件数据 */
    uint32_t base_addr = (type == FM_FILE_TYPE_SYSTEM) ?
                        FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;
    uint32_t write_addr = FM_SECTOR_TO_ADDR(base_addr, start_sector);

    /* 写入数据 */
    if (!fm_flash_write(write_addr, data, size)) {
        fm_free_sectors(type, start_sector, required_sectors);
        return false;
    }

    /* 创建或更新FAT表项 */
    fm_fat_entry_t *entry = existing_entry;
    if (!entry) {
        /* 查找空闲FAT表项 */
        for (uint32_t i = 0; i < FM_FAT_MAX_ENTRIES; i++) {
            if (g_fm_state.fat_entries[i].status == FM_FILE_STATUS_FREE) {
                entry = &g_fm_state.fat_entries[i];
                break;
            }
        }
        if (!entry) {
            fm_free_sectors(type, start_sector, required_sectors);
            return false; /* FAT表已满 */
        }
    }

    /* 填充FAT表项 */
    strncpy(entry->filename, filename, FM_FILENAME_MAX_LEN - 1);
    entry->filename[FM_FILENAME_MAX_LEN - 1] = '\0';
    entry->type = type;
    entry->status = FM_FILE_STATUS_USED;
    entry->start_sector = start_sector;
    entry->sector_count = required_sectors;
    entry->file_size = size;
    entry->crc16 = crc;
    entry->timestamp = 0; /* TODO: 获取实际时间戳 */

    /* 如果是新条目，增加计数 */
    if (existing_entry != entry) {
        g_fm_state.fat_header.entry_count++;
    }

    /* 更新使用扇区数 */
    if (type == FM_FILE_TYPE_SYSTEM) {
        g_fm_state.fat_header.system_used += required_sectors;
    } else {
        g_fm_state.fat_header.user_used += required_sectors;
    }

    /* 保存FAT表 */
    if (!fm_save_fat()) {
        fm_free_sectors(type, start_sector, required_sectors);
        return false;
    }

    /* 更新扇区位图 */
    if (!fm_update_sector_bitmap()) {
        return false;
    }

    return true;
}

/**
 * @brief 读取文件
 */
uint32_t fm_read(fm_file_handle_t *handle, uint8_t *buffer, uint32_t size)
{
    if (!handle || !handle->is_open || !buffer || size == 0) {
        return 0;
    }

    /* 计算可读取的字节数 */
    uint32_t remaining = handle->info.size - handle->current_pos;
    uint32_t to_read = (size < remaining) ? size : remaining;

    if (to_read == 0) {
        return 0;
    }

    /* 计算读取地址 */
    uint32_t read_addr = handle->info.start_addr + handle->current_pos;

    /* 读取数据 */
    if (!fm_flash_read(read_addr, buffer, to_read)) {
        return 0;
    }

    /* 更新读取位置 */
    handle->current_pos += to_read;

    return to_read;
}

/**
 * @brief 删除文件
 */
bool fm_delete(const char *filename, fm_file_type_t type)
{
	  extern void usb_printf(char *fmt, ...);
    if (!g_fm_state.initialized || !filename) {
        return false;
    }

    /* 查找FAT表项 */
    fm_fat_entry_t *entry = fm_find_fat_entry(filename, type);
    if (!entry || entry->status != FM_FILE_STATUS_USED) {
        return false;
    }

    /* 擦除文件数据 */
    uint32_t base_addr = (type == FM_FILE_TYPE_SYSTEM) ?
                        FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;
    uint32_t start_addr = FM_SECTOR_TO_ADDR(base_addr, entry->start_sector);
    for (uint32_t i = 0; i < entry->sector_count; i++) {
        if (!fm_flash_erase_sector(start_addr + i * FM_FLASH_SECTOR_SIZE)) {
            return false;
        }
    }

    /* 释放扇区 */
    fm_free_sectors(type, entry->start_sector, entry->sector_count);

    /* 更新FAT表项 */
    entry->status = FM_FILE_STATUS_FREE;
    g_fm_state.fat_header.entry_count--;

    /* 更新使用扇区数 */
    if (type == FM_FILE_TYPE_SYSTEM) {
        g_fm_state.fat_header.system_used -= entry->sector_count;
    } else {
        g_fm_state.fat_header.user_used -= entry->sector_count;
    }

    /* 保存FAT表 */
    if (!fm_save_fat()) {
        return false;
    }

    /* 更新扇区位图 */
    if (!fm_update_sector_bitmap()) {
        return false;
    }
    return true;
}

/**
 * @brief 获取文件信息
 */
bool fm_get_info(const char *filename, fm_file_type_t type, fm_file_info_t *info)
{
    if (!info || !g_fm_state.initialized || !filename) {
        return false;
    }

    /* 查找FAT表项 */
    fm_fat_entry_t *entry = fm_find_fat_entry(filename, type);
    if (!entry || entry->status != FM_FILE_STATUS_USED) {
        return false;
    }

    /* 填充信息 */
    strncpy(info->filename, entry->filename, FM_FILENAME_MAX_LEN - 1);
    info->filename[FM_FILENAME_MAX_LEN - 1] = '\0';
    info->type = type;
    info->status = entry->status;
    info->start_addr = FM_SECTOR_TO_ADDR(
        (type == FM_FILE_TYPE_SYSTEM) ? FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR,
        entry->start_sector
    );
    info->size = entry->file_size;
    info->sectors = entry->sector_count;
    info->crc16 = entry->crc16;
    info->timestamp = entry->timestamp;

    return true;
}

/**
 * @brief 获取文件数据地址
 */
uint32_t fm_get_data_addr(const char *filename, fm_file_type_t type)
{
    if (!g_fm_state.initialized || !filename) {
        return 0;
    }

    /* 查找FAT表项 */
    fm_fat_entry_t *entry = fm_find_fat_entry(filename, type);
    if (!entry || entry->status != FM_FILE_STATUS_USED) {
        return 0;
    }

    uint32_t base_addr = (type == FM_FILE_TYPE_SYSTEM) ?
                        FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;

    return FM_SECTOR_TO_ADDR(base_addr, entry->start_sector);
}

/**
 * @brief 获取区域使用情况
 */
bool fm_get_usage(fm_file_type_t type, uint32_t *used_files, uint32_t *free_files,
                  uint32_t *used_space, uint32_t *free_space)
{
    if (!g_fm_state.initialized) {
        return false;
    }

    uint32_t used = 0;
    uint32_t total_files = 0;
    uint32_t total_sectors = 0;
    uint32_t used_sectors = 0;

    if (type == FM_FILE_TYPE_SYSTEM) {
        total_files = FM_SYSTEM_MAX_FILES;
        total_sectors = FM_SYSTEM_MAX_SECTORS;
        used_sectors = g_fm_state.fat_header.system_used;

        /* 统计系统区已用文件数 */
        for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
            if (g_fm_state.fat_entries[i].type == FM_FILE_TYPE_SYSTEM &&
                g_fm_state.fat_entries[i].status == FM_FILE_STATUS_USED) {
                used++;
            }
        }
    } else {
        total_files = FM_USER_MAX_FILES;
        total_sectors = FM_USER_MAX_SECTORS;
        used_sectors = g_fm_state.fat_header.user_used;

        /* 统计用户区已用文件数 */
        for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
            if (g_fm_state.fat_entries[i].type == FM_FILE_TYPE_USER &&
                g_fm_state.fat_entries[i].status == FM_FILE_STATUS_USED) {
                used++;
            }
        }
    }

    if (used_files) *used_files = used;
    if (free_files) *free_files = total_files - used;
    if (used_space) *used_space = used_sectors * FM_FLASH_SECTOR_SIZE;
    if (free_space) *free_space = (total_sectors - used_sectors) * FM_FLASH_SECTOR_SIZE;

    return true;
}

/*=============================================================================
 *                          内部函数实现
 *============================================================================*/

/**
 * @brief 加载FAT表
 */
static bool fm_load_fat(void)
{
    fm_fat_header_t header;

    /* 读取FAT表头 */
	  uint32_t fat_base_addr = FM_FAT_BASE_ADDR;
    if (!fm_flash_read(fat_base_addr, (uint8_t*)&header, sizeof(header))) {
        return false;
    }

    /* 检查魔数 */
    if (header.magic != FM_FAT_MAGIC) {
        /* FAT表无效，初始化新的FAT表 */
        memset(&g_fm_state.fat_header, 0, sizeof(fm_fat_header_t));
        g_fm_state.fat_header.magic = FM_FAT_MAGIC;
        g_fm_state.fat_header.version = FM_FAT_VERSION;
        g_fm_state.fat_header.entry_count = 0;
        g_fm_state.fat_header.system_used = 0;
        g_fm_state.fat_header.user_used = 0;

        /* 初始化FAT表项 */
        for (uint32_t i = 0; i < FM_FAT_MAX_ENTRIES; i++) {
            g_fm_state.fat_entries[i].status = FM_FILE_STATUS_FREE;
        }

        /* 保存FAT表 */
        return fm_save_fat();
    }

    /* 检查版本号 */
    if ((header.version >> 16) != (FM_FAT_VERSION >> 16)) {
        /* 主版本号不兼容 */
        return false;
    }

    /* 读取FAT表项 */
    uint32_t entries_to_read = (header.entry_count < FM_FAT_MAX_ENTRIES) ?
                               header.entry_count : FM_FAT_MAX_ENTRIES;

    if (!fm_flash_read(FM_FAT_BASE_ADDR + sizeof(header),
                      (uint8_t*)g_fm_state.fat_entries,
                      entries_to_read * FM_FAT_ENTRY_SIZE)) {
        return false;
    }

    /* 保存表头 */
    g_fm_state.fat_header = header;

    return true;
}

/**
 * @brief 保存FAT表
 */
static bool fm_save_fat(void)
{
    /* 擦除FAT表扇区 */
    for (uint32_t i = 0; i < (FM_FAT_SIZE / FM_FLASH_SECTOR_SIZE); i++) {
        if (!fm_flash_erase_sector(FM_FAT_BASE_ADDR + i * FM_FLASH_SECTOR_SIZE)) {
            return false;
        }
    }

    /* 写入FAT表头 */
    if (!fm_flash_write(FM_FAT_BASE_ADDR,
                       (uint8_t*)&g_fm_state.fat_header,
                       sizeof(g_fm_state.fat_header))) {
        return false;
    }

    /* 写入FAT表项 */
    if (!fm_flash_write(FM_FAT_BASE_ADDR + sizeof(g_fm_state.fat_header),
                       (uint8_t*)g_fm_state.fat_entries,
                       g_fm_state.fat_header.entry_count * FM_FAT_ENTRY_SIZE)) {
        return false;
    }

    return true;
}

/**
 * @brief 更新扇区位图
 */
static bool fm_update_sector_bitmap(void)
{
    /* 初始化位图 */
    memset(g_fm_state.sector_bitmap_system, 0, sizeof(g_fm_state.sector_bitmap_system));
    memset(g_fm_state.sector_bitmap_user, 0, sizeof(g_fm_state.sector_bitmap_user));

    /* 遍历FAT表项，标记已使用的扇区 */
    for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
        fm_fat_entry_t *entry = &g_fm_state.fat_entries[i];
        if (entry->status != FM_FILE_STATUS_USED) {
            continue;
        }

        bool *bitmap = (entry->type == FM_FILE_TYPE_SYSTEM) ?
                      g_fm_state.sector_bitmap_system : g_fm_state.sector_bitmap_user;
        uint32_t max_sectors = (entry->type == FM_FILE_TYPE_SYSTEM) ?
                              FM_SYSTEM_MAX_SECTORS : FM_USER_MAX_SECTORS;

        /* 标记扇区 */
        for (uint32_t s = 0; s < entry->sector_count; s++) {
            uint32_t sector_idx = entry->start_sector + s;
            if (sector_idx >= max_sectors) {
                return false; /* 扇区索引越界 */
            }
            bitmap[sector_idx] = true;
        }
    }

    return true;
}

/**
 * @brief 查找空闲扇区
 */
static uint32_t fm_find_free_sectors(fm_file_type_t type, uint32_t sector_count)
{
    bool *bitmap = (type == FM_FILE_TYPE_SYSTEM) ?
                  g_fm_state.sector_bitmap_system : g_fm_state.sector_bitmap_user;
    uint32_t max_sectors = (type == FM_FILE_TYPE_SYSTEM) ?
                          FM_SYSTEM_MAX_SECTORS : FM_USER_MAX_SECTORS;

    if (sector_count == 0 || sector_count > max_sectors) {
        return 0xFFFFFFFF;
    }

    /* 查找连续的空闲扇区 */
    for (uint32_t i = 0; i <= max_sectors - sector_count; i++) {
        bool found = true;
        for (uint32_t j = 0; j < sector_count; j++) {
            if (bitmap[i + j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }

    return 0xFFFFFFFF; /* 未找到 */
}

/**
 * @brief 分配扇区
 */
static bool fm_allocate_sectors(fm_file_type_t type, uint32_t start_sector, uint32_t sector_count)
{
    bool *bitmap = (type == FM_FILE_TYPE_SYSTEM) ?
                  g_fm_state.sector_bitmap_system : g_fm_state.sector_bitmap_user;
    uint32_t max_sectors = (type == FM_FILE_TYPE_SYSTEM) ?
                          FM_SYSTEM_MAX_SECTORS : FM_USER_MAX_SECTORS;

    if (start_sector + sector_count > max_sectors) {
        return false;
    }

    /* 标记扇区为已使用 */
    for (uint32_t i = 0; i < sector_count; i++) {
        bitmap[start_sector + i] = true;
    }

    return true;
}

/**
 * @brief 释放扇区
 */
static bool fm_free_sectors(fm_file_type_t type, uint32_t start_sector, uint32_t sector_count)
{
    bool *bitmap = (type == FM_FILE_TYPE_SYSTEM) ?
                  g_fm_state.sector_bitmap_system : g_fm_state.sector_bitmap_user;
    uint32_t max_sectors = (type == FM_FILE_TYPE_SYSTEM) ?
                          FM_SYSTEM_MAX_SECTORS : FM_USER_MAX_SECTORS;

    if (start_sector + sector_count > max_sectors) {
        return false;
    }

    /* 标记扇区为空闲 */
    for (uint32_t i = 0; i < sector_count; i++) {
        bitmap[start_sector + i] = false;
    }

    return true;
}

/**
 * @brief 查找FAT表项
 */
static fm_fat_entry_t* fm_find_fat_entry(const char *filename, fm_file_type_t type)
{
    if (!filename) {
        return NULL;
    }

    for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
        fm_fat_entry_t *entry = &g_fm_state.fat_entries[i];
        if (entry->type == type &&
            entry->status == FM_FILE_STATUS_USED &&
            strcmp(entry->filename, filename) == 0) {
            return entry;
        }
    }
    return NULL;
}

/**
 * @brief 验证文件完整性
 */
static bool fm_validate_file(const fm_fat_entry_t *entry)
{
    if (!entry || entry->status != FM_FILE_STATUS_USED) {
        return false;
    }

    /* 计算文件数据地址 */
    uint32_t base_addr = (entry->type == FM_FILE_TYPE_SYSTEM) ?
                        FM_SYSTEM_ARRAY_BASE_ADDR : FM_USER_ARRAY_BASE_ADDR;
    uint32_t file_addr = FM_SECTOR_TO_ADDR(base_addr, entry->start_sector);

    /* 分配读取缓冲区 */
    uint8_t *buffer = (uint8_t*)fm_malloc(entry->file_size);
    if (!buffer) {
        return false;
    }

    /* 读取文件数据 */
    bool read_ok = fm_flash_read(file_addr, buffer, entry->file_size);
    if (!read_ok) {
        fm_free(buffer);
        return false;
    }

    /* 计算CRC16并比较 */
    uint16_t calculated_crc = fm_crc16(buffer, entry->file_size);
    bool crc_ok = (calculated_crc == entry->crc16);

    fm_free(buffer);
    return crc_ok;
}

/**
 * @brief 计算需要的扇区数
 */
static uint32_t fm_calculate_required_sectors(uint32_t file_size)
{
    if (file_size == 0) {
        return 0;
    }
    return (file_size + FM_FLASH_SECTOR_SIZE - 1) / FM_FLASH_SECTOR_SIZE;
}

/**
 * @brief 检查字符是否合法的文件名字符
 */
static bool fm_is_valid_filename_char(char c)
{
    /* 允许的字符：A-Z, a-z, 0-9, 下划线_, 点., 连字符- */
    if ((c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '.' || c == '-') {
        return true;
    }
    return false;
}

/**
 * @brief 验证文件名是否合法
 */
bool fm_validate_filename(const char *filename)
{
    if (!filename || filename[0] == '\0') {
        return false; /* 空文件名无效 */
    }

    size_t len = strlen(filename);
    if (len >= FM_FILENAME_MAX_LEN) {
        return false; /* 文件名太长 */
    }

    /* 检查每个字符 */
    for (size_t i = 0; i < len; i++) {
        if (!fm_is_valid_filename_char(filename[i])) {
            return false;
        }
    }

    /* 文件名不能以点开头（隐藏文件）或包含连续的点 */
    if (filename[0] == '.') {
        return false;
    }

    /* 检查是否包含 ".." （目录遍历） */
    if (strstr(filename, "..") != NULL) {
        return false;
    }

    return true;
}

/**
 * @brief 获取文件列表
 */
uint32_t fm_list_files(fm_file_type_t type, char list[][FM_FILENAME_MAX_LEN], uint32_t max_count)
{
    if (!list || max_count == 0 || !g_fm_state.initialized) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_fm_state.fat_header.entry_count; i++) {
        fm_fat_entry_t *entry = &g_fm_state.fat_entries[i];
        if (entry->type == type && entry->status == FM_FILE_STATUS_USED) {
            if (count < max_count) {
                strncpy(list[count], entry->filename, FM_FILENAME_MAX_LEN - 1);
                list[count][FM_FILENAME_MAX_LEN - 1] = '\0';
                count++;
            }
        }
    }

    return count;
}

/**
 * @brief 检查文件是否存在
 */
bool fm_file_exists(const char *filename, fm_file_type_t type)
{
    if (!filename || !g_fm_state.initialized) {
        return false;
    }

    fm_fat_entry_t *entry = fm_find_fat_entry(filename, type);
    return (entry != NULL && entry->status == FM_FILE_STATUS_USED);
}
void fm_print_simple_usage(void)
{
	   extern void usb_printf(char *fmt, ...);
    if (!g_fm_state.initialized) {
        usb_printf("File system not initialized\r\n");
        return;
    }

    uint32_t sys_used_files, sys_free_files, sys_used_space, sys_free_space;
    uint32_t user_used_files, user_free_files, user_used_space, user_free_space;

    /* Get system area usage */
    fm_get_usage(FM_FILE_TYPE_SYSTEM, &sys_used_files, &sys_free_files,
                 &sys_used_space, &sys_free_space);

    /* Get user area usage */
    fm_get_usage(FM_FILE_TYPE_USER, &user_used_files, &user_free_files,
                 &user_used_space, &user_free_space);

    /* Print header */
    usb_printf("\r\n=== File System Usage ===\r\n");

    /* System Area */
    usb_printf("System Area:\r\n");
    usb_printf("  Files: %u/%u (used/max)\r\n", sys_used_files, FM_SYSTEM_MAX_FILES);
    usb_printf("  Space: %u/%u KB (used/total)\r\n",
              sys_used_space / 1024, (sys_used_space + sys_free_space) / 1024);

    /* System area file list */
    if (sys_used_files > 0) {
        usb_printf("  File list:\r\n");
        char sys_file_list[FM_SYSTEM_MAX_FILES][FM_FILENAME_MAX_LEN];
        uint32_t sys_file_count = fm_list_files(FM_FILE_TYPE_SYSTEM, sys_file_list, FM_SYSTEM_MAX_FILES);

        for (uint32_t i = 0; i < sys_file_count; i++) {
            usb_printf("    - %s\r\n", sys_file_list[i]);
        }
    } else {
        usb_printf("  File list: (empty)\r\n");
    }

    usb_printf("\r\n");

    /* User Area */
    usb_printf("User Area:\r\n");
    usb_printf("  Files: %u/%u (used/max)\r\n", user_used_files, FM_USER_MAX_FILES);
    usb_printf("  Space: %u/%u KB (used/total)\r\n",
              user_used_space / 1024, (user_used_space + user_free_space) / 1024);

    /* User area file list */
    if (user_used_files > 0) {
        usb_printf("  File list:\r\n");
        char user_file_list[FM_USER_MAX_FILES][FM_FILENAME_MAX_LEN];
        uint32_t user_file_count = fm_list_files(FM_FILE_TYPE_USER, user_file_list, FM_USER_MAX_FILES);

        for (uint32_t i = 0; i < user_file_count; i++) {
            usb_printf("    - %s\r\n", user_file_list[i]);
        }
    } else {
        usb_printf("  File list: (empty)\r\n");
    }

    usb_printf("\r\n");
}
/*=============================================================================
 *                          底层驱动桩函数
 *============================================================================*/
static uint8_t flash_page_buffer[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
static uint16_t flash_halfword_buffer[FLASH_PAGE_SIZE/2];
/**
  * @brief  字节数组转半字数组（通用转换）
  * @param  halfwords: 半字输出缓冲区
  * @param  bytes: 字节输入数据
  * @param  byte_len: 字节长度
  * @param  fill_value: 填充值（0xFF表示未使用区域）
  * @retval 转换后的半字数
  */
static uint32_t bytes_to_halfwords(uint16_t *halfwords, const uint8_t *bytes, 
                           uint32_t byte_len, uint8_t fill_value)
{
    uint32_t i;
    uint32_t halfword_count = (byte_len + 1) / 2;  // 向上取整
    
    // 初始化半字缓冲区
    for(i = 0; i < halfword_count; i++)
    {
        halfwords[i] = (fill_value << 8) | fill_value;  // 填充0xFFFF
    }
    
    // 填充数据
    for(i = 0; i < byte_len; i++)
    {
        uint32_t halfword_idx = i / 2;
        uint8_t byte_pos = i % 2;
        
        if(byte_pos == 0)
        {
            // 低字节
            halfwords[halfword_idx] = (halfwords[halfword_idx] & 0xFF00) | bytes[i];
        }
        else
        {
            // 高字节
            halfwords[halfword_idx] = (halfwords[halfword_idx] & 0x00FF) | (bytes[i] << 8);
        }
    }
    
    return halfword_count;
}

 uint32_t FLASH_ReadBytes(uint32_t addr, uint8_t *buffer, uint32_t byte_len)
{
    uint32_t i;
    
    if(!buffer || byte_len == 0)
        return 0;
    
    // 检查地址范围
    if(addr < FM_FLASH_BASE_ADDR)
        return 0;
    
    // 直接按字节读取（最简单直接的方式）
    for(i = 0; i < byte_len; i++)
    {
        // 计算对应的半字地址
        uint32_t halfword_addr = (addr + i) & ~0x01;
        uint16_t halfword_value = *(volatile uint16_t*)halfword_addr;
        
        // 提取相应的字节
        if((addr + i) & 0x01)
        {
            // 奇数地址，取高字节
            buffer[i] = (uint8_t)(halfword_value >> 8);
        }
        else
        {
            // 偶数地址，取低字节
            buffer[i] = (uint8_t)(halfword_value & 0xFF);
        }
    }
    
    return byte_len;
}
 /**
  * @brief  写入任意字节数据（自动处理扇区擦除）
  * @param  addr: 写入地址（任意地址）
  * @param  data: 写入数据
  * @param  byte_len: 数据长度
  * @retval 实际写入的字节数
  */
uint32_t FLASH_WriteBytes(uint32_t addr, const uint8_t *data, uint32_t byte_len)
{
    uint32_t bytes_written = 0;
    uint32_t current_addr = addr;
    uint32_t current_len = byte_len;
    const uint8_t *current_data = data;
    
    if(!data || byte_len == 0)
        return 0;
    
    // 按扇区循环处理
    while(current_len > 0)
    {
        // 获取当前扇区信息
        uint32_t page_addr = current_addr & ~(FLASH_PAGE_SIZE - 1);
        uint32_t page_offset = current_addr - page_addr;
        
        // 计算本次可写入的长度（不超过当前扇区）
        uint32_t write_len = current_len;
        if(page_offset + write_len > FLASH_PAGE_SIZE)
            write_len = FLASH_PAGE_SIZE - page_offset;
        
        // 读取整个扇区
        FLASH_ReadBytes(page_addr, flash_page_buffer, FLASH_PAGE_SIZE);
        
        // 修改需要写入的部分
        memcpy(&flash_page_buffer[page_offset], current_data, write_len);
        
        // 将字节转换为半字
        uint32_t halfword_count = bytes_to_halfwords(
            flash_halfword_buffer, 
            flash_page_buffer, 
            FLASH_PAGE_SIZE,
            0xFF  // 未使用区域填充0xFF
        );
        
        // 写入整个扇区（会自动擦除）
        stmflash_write(page_addr, flash_halfword_buffer, halfword_count);
        
        // 更新进度
        bytes_written += write_len;
        current_addr += write_len;
        current_data += write_len;
        current_len -= write_len;
    }
    
    return bytes_written;
}
/**
 * @brief Flash扇区擦除（需要用户实现）
 */
__attribute__((weak)) bool fm_flash_erase_sector(uint32_t addr)
{
			flash_erase_sector((addr - FM_FLASH_BASE_ADDR) / STM32_SECTOR_SIZE);
    return true;
}

// 读取函数 - 支持任意对齐，但要求调用者保证数据完整性
__attribute__((weak)) bool fm_flash_read(uint32_t addr, uint8_t *data, uint32_t size)
{
     
    FLASH_ReadBytes(addr,data,size);
 
    return true;
}
 
__attribute__((weak)) bool fm_flash_write(uint32_t addr, const uint8_t *data, uint32_t size)
{
    FLASH_WriteBytes(addr,data,size);
    return true;
}

/**
 * @brief 内存申请（需要用户实现）
 */
__attribute__((weak)) void* fm_malloc(uint32_t size)
{
    return mymalloc(SRAMIN,size);
}

/**
 * @brief 内存释放（需要用户实现）
 */
__attribute__((weak)) void fm_free(void *ptr)
{
    /* 用户需要根据具体的内存管理实现此函数 */
    myfree(SRAMIN,ptr);
}

/**
 * @brief CRC16计算（需要用户实现）
 */
__attribute__((weak)) uint16_t fm_crc16(const uint8_t *data, uint32_t size)
{
   uint16_t crc = 0xFFFF;
    
    while (size--) {
        crc ^= *data++ << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}