/**
 * @file w25q80.h
 * @brief W25Q80DVSNIG SPI Flash Driver
 *
 * This driver provides low-level SPI interface and FATFS disk I/O interface.
 * User must implement SPI hardware abstraction functions.
 *
 * @version 1.0.0
 * @date 2026-01-22
 */

#ifndef W25Q80_H
#define W25Q80_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory size definitions */
#define W25Q80_FLASH_SIZE          (1024 * 1024)    /* 8 Mbit = 1 MB */
#define W25Q80_PAGE_SIZE           256              /* Page size in bytes */
#define W25Q80_SECTOR_SIZE         4096             /* Sector size in bytes */
#define W25Q80_BLOCK_SIZE          65536            /* Block size (64KB) */
#define W25Q80_NUM_SECTORS         (W25Q80_FLASH_SIZE / W25Q80_SECTOR_SIZE)
#define W25Q80_NUM_PAGES           (W25Q80_FLASH_SIZE / W25Q80_PAGE_SIZE)

/* Command definitions */
#define W25Q80_CMD_WRITE_ENABLE    0x06
#define W25Q80_CMD_WRITE_DISABLE   0x04
#define W25Q80_CMD_READ_STATUS1    0x05
#define W25Q80_CMD_READ_STATUS2    0x35
#define W25Q80_CMD_WRITE_STATUS    0x01
#define W25Q80_CMD_PAGE_PROGRAM    0x02
#define W25Q80_CMD_QUAD_PAGE_PROGRAM 0x32
#define W25Q80_CMD_BLOCK_ERASE_4K  0x20
#define W25Q80_CMD_BLOCK_ERASE_32K 0x52
#define W25Q80_CMD_BLOCK_ERASE_64K 0xD8
#define W25Q80_CMD_CHIP_ERASE      0xC7
#define W25Q80_CMD_ERASE_SUSPEND   0x75
#define W25Q80_CMD_ERASE_RESUME    0x7A
#define W25Q80_CMD_POWER_DOWN      0xB9
#define W25Q80_CMD_HIGH_PERF_MODE  0xA3
#define W25Q80_CMD_CONT_MODE_RESET 0xFF
#define W25Q80_CMD_RELEASE_PDOWN   0xAB
#define W25Q80_CMD_MANUF_DEVICE_ID 0x90
#define W25Q80_CMD_JEDEC_ID        0x9F
#define W25Q80_CMD_READ_UNIQUE_ID  0x4B
#define W25Q80_CMD_READ_DATA       0x03
#define W25Q80_CMD_FAST_READ       0x0B
#define W25Q80_CMD_FAST_READ_DUAL  0x3B
#define W25Q80_CMD_FAST_READ_QUAD  0x6B

/* Status register bits */
#define W25Q80_STATUS_BUSY         (1 << 0)
#define W25Q80_STATUS_WEL          (1 << 1)
#define W25Q80_STATUS_BP0          (1 << 2)
#define W25Q80_STATUS_BP1          (1 << 3)
#define W25Q80_STATUS_BP2          (1 << 4)
#define W25Q80_STATUS_TB           (1 << 5)
#define W25Q80_STATUS_SEC          (1 << 6)
#define W25Q80_STATUS_SRP0         (1 << 7)

/* Manufacturer and device ID */
#define W25Q80_MANUFACTURER_ID     0xEF
#define W25Q80_DEVICE_ID           0x13  /* W25Q80DVSNIG: 0x13 for 8Mbit */

/**
 * @brief SPI hardware abstraction structure
 *
 * User must implement these functions for specific SPI hardware.
 */
typedef struct {
    /**
     * @brief Initialize SPI peripheral
     * @return true if success, false otherwise
     */
    bool (*init)(void);

    /**
     * @brief Deinitialize SPI peripheral
     */
    void (*deinit)(void);

    /**
     * @brief Select chip (set CS low)
     */
    void (*select)(void);

    /**
     * @brief Deselect chip (set CS high)
     */
    void (*deselect)(void);

    /**
     * @brief Transmit and receive one byte
     * @param data Byte to transmit
     * @return Received byte
     */
    uint8_t (*transfer)(uint8_t data);

    /**
     * @brief Transmit multiple bytes (TX only)
     * @param data Pointer to transmit buffer
     * @param length Number of bytes to transmit
     */
    void (*transmit)(const uint8_t *data, uint32_t length);

    /**
     * @brief Receive multiple bytes (RX only)
     * @param data Pointer to receive buffer
     * @param length Number of bytes to receive
     */
    void (*receive)(uint8_t *data, uint32_t length);

    /**
     * @brief Transmit and receive multiple bytes (full duplex)
     * @param tx_data Pointer to transmit buffer
     * @param rx_data Pointer to receive buffer
     * @param length Number of bytes to transfer
     */
    void (*transfer_full)(const uint8_t *tx_data, uint8_t *rx_data, uint32_t length);
} w25q80_spi_t;

/**
 * @brief Driver context structure
 */
typedef struct {
    const w25q80_spi_t *spi;      /**< SPI hardware abstraction */
    bool initialized;             /**< Driver initialization flag */
    uint32_t sector_size;         /**< Sector size (typically 4096) */
    uint32_t sector_count;        /**< Total number of sectors */
} w25q80_t;

/* ==================== Basic SPI Interface Functions ==================== */

/**
 * @brief Initialize W25Q80 driver
 * @param dev Pointer to driver context
 * @param spi Pointer to SPI hardware abstraction
 * @return true if initialization successful, false otherwise
 */
bool w25q80_init(w25q80_t *dev, const w25q80_spi_t *spi);

/**
 * @brief Deinitialize W25Q80 driver
 * @param dev Pointer to driver context
 */
void w25q80_deinit(w25q80_t *dev);

/**
 * @brief Read manufacturer and device ID using manufacturer device ID command (0x90)
 * @param dev Pointer to driver context
 * @param manufacturer_id Pointer to store manufacturer ID (optional)
 * @param device_id Pointer to store device ID (optional)
 * @return true if ID read successful, false otherwise
 */
bool w25q80_read_id(w25q80_t *dev, uint8_t *manufacturer_id, uint8_t *device_id);

/**
 * @brief Read JEDEC ID (manufacturer ID, memory type, capacity ID)
 * @param dev Pointer to driver context
 * @param manufacturer_id Pointer to store manufacturer ID (optional)
 * @param memory_type Pointer to store memory type (optional)
 * @param capacity_id Pointer to store capacity ID (optional)
 * @return true if ID read successful, false otherwise
 */
bool w25q80_read_jedec_id(w25q80_t *dev, uint8_t *manufacturer_id, uint8_t *memory_type, uint8_t *capacity_id);

/**
 * @brief Read status register 1
 * @param dev Pointer to driver context
 * @return Status register value
 */
uint8_t w25q80_read_status1(w25q80_t *dev);

/**
 * @brief Read status register 2
 * @param dev Pointer to driver context
 * @return Status register 2 value
 */
uint8_t w25q80_read_status2(w25q80_t *dev);

/**
 * @brief Wait until flash is not busy
 * @param dev Pointer to driver context
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return true if flash ready, false if timeout
 */
bool w25q80_wait_busy(w25q80_t *dev, uint32_t timeout_ms);

/**
 * @brief Enable write operations
 * @param dev Pointer to driver context
 */
void w25q80_write_enable(w25q80_t *dev);

/**
 * @brief Disable write operations
 * @param dev Pointer to driver context
 */
void w25q80_write_disable(w25q80_t *dev);

/**
 * @brief Erase a 4KB sector
 * @param dev Pointer to driver context
 * @param sector_addr Sector address (must be sector aligned)
 * @return true if erase command accepted, false otherwise
 */
bool w25q80_erase_sector(w25q80_t *dev, uint32_t sector_addr);

/**
 * @brief Erase a 32KB block
 * @param dev Pointer to driver context
 * @param block_addr Block address (must be 32KB aligned)
 * @return true if erase command accepted, false otherwise
 */
bool w25q80_erase_block_32k(w25q80_t *dev, uint32_t block_addr);

/**
 * @brief Erase a 64KB block
 * @param dev Pointer to driver context
 * @param block_addr Block address (must be 64KB aligned)
 * @return true if erase command accepted, false otherwise
 */
bool w25q80_erase_block_64k(w25q80_t *dev, uint32_t block_addr);

/**
 * @brief Erase entire chip
 * @param dev Pointer to driver context
 * @return true if erase command accepted, false otherwise
 */
bool w25q80_erase_chip(w25q80_t *dev);

/**
 * @brief Read data from flash
 * @param dev Pointer to driver context
 * @param addr Starting address (0 to FLASH_SIZE-1)
 * @param data Pointer to buffer to store read data
 * @param length Number of bytes to read
 * @return true if read successful, false otherwise
 */
bool w25q80_read(w25q80_t *dev, uint32_t addr, uint8_t *data, uint32_t length);

/**
 * @brief Write data to flash (page program)
 * @note Address must be page aligned, and length must not exceed page boundary
 * @param dev Pointer to driver context
 * @param addr Starting address (must be page aligned)
 * @param data Pointer to data to write
 * @param length Number of bytes to write (max 256 per page)
 * @return true if write successful, false otherwise
 */
bool w25q80_write_page(w25q80_t *dev, uint32_t addr, const uint8_t *data, uint32_t length);

/**
 * @brief Write data to flash with automatic page handling
 * @param dev Pointer to driver context
 * @param addr Starting address (any alignment)
 * @param data Pointer to data to write
 * @param length Number of bytes to write
 * @return true if write successful, false otherwise
 */
bool w25q80_write(w25q80_t *dev, uint32_t addr, const uint8_t *data, uint32_t length);

/**
 * @brief Check if address range is erased (all 0xFF)
 * @param dev Pointer to driver context
 * @param addr Starting address
 * @param length Number of bytes to check
 * @return true if all bytes are 0xFF, false otherwise
 */
bool w25q80_is_erased(w25q80_t *dev, uint32_t addr, uint32_t length);

/* ==================== FATFS Disk I/O Interface ==================== */

/**
 * @brief FATFS disk initialize function
 * @param pdrv Physical drive number (0..)
 * @return Disk status (0: OK, 1: NOT READY, 2: WRITE PROTECTED)
 */
int w25q80_disk_initialize(uint8_t pdrv);

/**
 * @brief FATFS disk status function
 * @param pdrv Physical drive number (0..)
 * @return Disk status (0: OK, 1: NOT READY, 2: WRITE PROTECTED)
 */
int w25q80_disk_status(uint8_t pdrv);

/**
 * @brief FATFS disk read function
 * @param pdrv Physical drive number (0..)
 * @param buff Pointer to buffer to store read data
 * @param sector Sector address (LBA)
 * @param count Number of sectors to read
 * @return Result (0: OK, 1: ERROR, 2: NOT READY, 3: PARAMETER ERROR)
 */
int w25q80_disk_read(uint8_t pdrv, uint8_t *buff, uint32_t sector, uint32_t count);

/**
 * @brief FATFS disk write function
 * @param pdrv Physical drive number (0..)
 * @param buff Pointer to data to write
 * @param sector Sector address (LBA)
 * @param count Number of sectors to write
 * @return Result (0: OK, 1: ERROR, 2: NOT READY, 3: PARAMETER ERROR)
 */
int w25q80_disk_write(uint8_t pdrv, const uint8_t *buff, uint32_t sector, uint32_t count);

/**
 * @brief FATFS disk I/O control function
 * @param pdrv Physical drive number (0..)
 * @param cmd Control command
 * @param buff Pointer to parameter buffer
 * @return Result (0: OK, 1: ERROR, 2: PARAMETER ERROR)
 */
int w25q80_disk_ioctl(uint8_t pdrv, uint8_t cmd, void *buff);

/* ==================== Global Driver Instance ==================== */
void usb_printf(char *fmt, ...);
void extern_flash_Init(void);
/**
 * @brief Global driver instance
 *
 * User must set spi field before calling w25q80_init().
 */
extern w25q80_t w25q80_drv;

#ifdef __cplusplus
}
#endif

#endif /* W25Q80_H */