/**
 * @file file_manager.h
 * @brief STM32F103RCT6小型文件系统
 *
 * 该文件系统分为系统区和用户区两个区域：
 * - 系统区：每个文件固定占用2KB（一个扇区），最大文件数量可配置
 * - 用户区：文件大小可配置，按扇区分配（1个文件占用整数个扇区）
 *
 * 文件分配表（FAT）存储在Flash固定区域，每个文件有CRC16校验。
 */

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif
#include "stdlib.h"
#include "stmflash.h"
#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 *                          配置文件系统参数
 *============================================================================*/

/**
 * @brief Flash扇区大小（字节）
 *
 * STM32F103RCT6的Flash扇区大小为2KB（2048字节）
 */
#define FM_FLASH_SECTOR_SIZE          STM32_SECTOR_SIZE

/**
 * @brief Flash总大小（字节）
 *
 * STM32F103RCT6有256KB Flash
 */
#define FM_FLASH_TOTAL_SIZE           (STM32_FLASH_SIZE)

/**
 * @brief Flash起始地址
 */
#define FM_FLASH_BASE_ADDR            STM32_FLASH_BASE

/**
 * @brief 系统区配置
 */
#define FM_SYSTEM_ARRAY_BASE_ADDR     (FM_FLASH_BASE_ADDR + 0x0000U)    /* 系统区起始地址 */
#define FM_SYSTEM_ARRAY_SIZE          (16U * 1024U)                     /* 系统区大小（64KB） */
#define FM_SYSTEM_MAX_FILES           8U                               /* 系统区最大文件数 */
#define FM_SYSTEM_FILE_SIZE           FM_FLASH_SECTOR_SIZE              /* 系统区文件大小（2KB） */

#define FM_FILENAME_MAX_LEN          16U  /* 文件名最大长度（包含结束符） */

/**
 * @brief 用户区配置
 */
#define FM_USER_ARRAY_BASE_ADDR      (FM_SYSTEM_ARRAY_BASE_ADDR + FM_SYSTEM_ARRAY_SIZE) /* 用户区起始地址 */
#define FM_USER_ARRAY_SIZE           (FM_FLASH_TOTAL_SIZE - FM_SYSTEM_ARRAY_SIZE - FM_FAT_SIZE)        /* 用户区大小（192KB） */
#define FM_USER_MAX_FILES            1U                                                 /* 用户区最大文件数 */
#define FM_USER_MAX_FILE_SIZE        (6U * FM_FLASH_SECTOR_SIZE)                        /* 用户区最大文件大小（32KB） */

/**
 * @brief 文件分配表（FAT）配置
 */
#define FM_FAT_BASE_ADDR             ((FM_FLASH_BASE_ADDR + FM_FLASH_TOTAL_SIZE) - (4 * FM_FLASH_SECTOR_SIZE)) /* FAT表在最后4个扇区 */
#define FM_FAT_SIZE                  (4U * FM_FLASH_SECTOR_SIZE)                                           /* FAT表大小（8KB） */
/**
 * @brief 文件类型枚举
 */
typedef enum {
    FM_FILE_TYPE_SYSTEM = 0,    /* 系统文件 */
    FM_FILE_TYPE_USER = 1       /* 用户文件 */
} fm_file_type_t;

/**
 * @brief 文件状态枚举
 */
typedef enum {
    FM_FILE_STATUS_FREE = 0,    /* 空闲 */
    FM_FILE_STATUS_USED = 1,    /* 已使用 */
    FM_FILE_STATUS_CORRUPT = 2  /* 损坏 */
} fm_file_status_t;

/**
 * @brief 文件信息结构体
 */
typedef struct {
    char filename[FM_FILENAME_MAX_LEN];  /* 文件名（以'\\0'结束） */
    fm_file_type_t type;                 /* 文件类型：系统/用户 */
    fm_file_status_t status;             /* 文件状态 */
    uint32_t start_addr;                 /* 文件起始地址 */
    uint32_t size;                       /* 文件大小（字节） */
    uint32_t sectors;                    /* 占用扇区数 */
    uint16_t crc16;                      /* CRC16校验值 */
    uint32_t timestamp;                  /* 时间戳（可选） */
} fm_file_info_t;

typedef struct {
    uint32_t used_files;        /* 已使用文件数 */
    uint32_t free_files;        /* 空闲文件数 */
    uint32_t max_files;         /* 最大文件数 */
    uint32_t used_space;        /* 已使用空间（字节） */
    uint32_t free_space;        /* 空闲空间（字节） */
    uint32_t total_space;       /* 总空间（字节） */
    uint32_t used_sectors;      /* 已使用扇区数 */
    uint32_t free_sectors;      /* 空闲扇区数 */
    uint32_t total_sectors;     /* 总扇区数 */
    float usage_percentage;     /* 使用率（百分比） */
} fm_usage_detail_t;

typedef struct {
    fm_usage_detail_t system;   /* 系统区使用情况 */
    fm_usage_detail_t user;     /* 用户区使用情况 */
    fm_usage_detail_t total;    /* 总计使用情况 */
} fm_all_usage_t;

/**
 * @brief 文件句柄
 */
typedef struct {
    fm_file_info_t info;        /* 文件信息 */
    uint32_t current_pos;       /* 当前读写位置 */
    bool is_open;               /* 是否已打开 */
} fm_file_handle_t;

/*=============================================================================
 *                              文件系统API
 *============================================================================*/

/**
 * @brief 初始化文件系统
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool fm_init(void);

/**
 * @brief 格式化指定区域
 *
 * @param type 区域类型：系统区或用户区
 * @return true 格式化成功
 * @return false 格式化失败
 */
bool fm_format(fm_file_type_t type);

/**
 * @brief 打开文件
 *
 * @param filename 文件名（以'\\0'结束）
 * @param type 文件类型
 * @param handle 返回的文件句柄指针
 * @return true 打开成功
 * @return false 打开失败（文件不存在或损坏）
 */
bool fm_open(const char *filename, fm_file_type_t type, fm_file_handle_t *handle);

/**
 * @brief 关闭文件
 *
 * @param handle 文件句柄指针
 * @return true 关闭成功
 * @return false 关闭失败
 */
bool fm_close(fm_file_handle_t *handle);

/**
 * @brief 创建/写入文件
 *
 * @param filename 文件名（以'\\0'结束）
 * @param type 文件类型
 * @param data 文件数据指针
 * @param size 文件大小（字节）
 * @return true 写入成功
 * @return false 写入失败
 */
bool fm_write(const char *filename, fm_file_type_t type, const uint8_t *data, uint32_t size);

/**
 * @brief 读取文件
 *
 * @param handle 文件句柄
 * @param buffer 读取缓冲区
 * @param size 要读取的字节数
 * @return uint32_t 实际读取的字节数
 */
uint32_t fm_read(fm_file_handle_t *handle, uint8_t *buffer, uint32_t size);

/**
 * @brief 删除文件
 *
 * @param filename 文件名（以'\\0'结束）
 * @param type 文件类型
 * @return true 删除成功
 * @return false 删除失败
 */
bool fm_delete(const char *filename, fm_file_type_t type);

/**
 * @brief 获取文件信息
 *
 * @param filename 文件名（以'\\0'结束）
 * @param type 文件类型
 * @param info 返回的文件信息指针
 * @return true 获取成功
 * @return false 获取失败（文件不存在）
 */
bool fm_get_info(const char *filename, fm_file_type_t type, fm_file_info_t *info);

/**
 * @brief 获取文件数据地址
 *
 * @param filename 文件名（以'\\0'结束）
 * @param type 文件类型
 * @return uint32_t 文件数据起始地址，0表示失败
 */
uint32_t fm_get_data_addr(const char *filename, fm_file_type_t type);

/**
 * @brief 获取区域使用情况
 *
 * @param type 区域类型
 * @param used_files 返回的已使用文件数
 * @param free_files 返回的空闲文件数
 * @param used_space 返回的已使用空间（字节）
 * @param free_space 返回的空闲空间（字节）
 * @return true 获取成功
 * @return false 获取失败
 */
bool fm_get_usage(fm_file_type_t type, uint32_t *used_files, uint32_t *free_files,
                  uint32_t *used_space, uint32_t *free_space);

/**
 * @brief 验证文件名是否合法
 *
 * @param filename 文件名
 * @return true 文件名合法
 * @return false 文件名不合法
 */
bool fm_validate_filename(const char *filename);

/**
 * @brief 获取文件列表
 *
 * @param type 文件类型
 * @param list 文件名列表缓冲区
 * @param max_count 最大文件数
 * @return uint32_t 实际获取的文件数
 */
uint32_t fm_list_files(fm_file_type_t type, char list[][FM_FILENAME_MAX_LEN], uint32_t max_count);

/**
 * @brief 检查文件是否存在
 *
 * @param filename 文件名
 * @param type 文件类型
 * @return true 文件存在
 * @return false 文件不存在
 */
bool fm_file_exists(const char *filename, fm_file_type_t type);


void fm_print_simple_usage(void);
 
/*=============================================================================
 *                          底层驱动API（需要用户实现）
 *============================================================================*/

/**
 * @brief Flash扇区擦除
 *
 * @param addr 扇区起始地址
 * @return true 擦除成功
 * @return false 擦除失败
 */
bool fm_flash_erase_sector(uint32_t addr);

/**
 * @brief Flash数据读取
 *
 * @param addr 读取地址
 * @param data 数据缓冲区
 * @param size 读取大小（字节）
 * @return true 读取成功
 * @return false 读取失败
 */
bool fm_flash_read(uint32_t addr, uint8_t *data, uint32_t size);

/**
 * @brief Flash数据写入
 *
 * @param addr 写入地址
 * @param data 数据缓冲区
 * @param size 写入大小（字节）
 * @return true 写入成功
 * @return false 写入失败
 */
bool fm_flash_write(uint32_t addr, const uint8_t *data, uint32_t size);

/**
 * @brief 内存申请（用于读写缓冲区）
 *
 * @param size 申请大小（字节）
 * @return void* 内存指针，NULL表示失败
 */
void* fm_malloc(uint32_t size);

/**
 * @brief 内存释放
 *
 * @param ptr 内存指针
 */
void fm_free(void *ptr);

/**
 * @brief CRC16计算
 *
 * @param data 数据指针
 * @param size 数据大小
 * @return uint16_t CRC16值
 */
uint16_t fm_crc16(const uint8_t *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* FILE_MANAGER_H */