/**
 * @file w25q80.c
 * @brief W25Q80DVSNIG SPI Flash Driver Implementation
 *
 * @version 1.0.0
 * @date 2026-01-22
 */

#include "w25q80.h"
#include "spi.h"
#include <string.h>
 


/* Global driver instance */
w25q80_t w25q80_drv = {
    .spi = NULL,
    .initialized = false,
    .sector_size = W25Q80_SECTOR_SIZE,
    .sector_count = W25Q80_NUM_SECTORS
};
static const w25q80_spi_t example_spi;
/* Private function prototypes */
static void send_command_addr(w25q80_t *dev, uint8_t cmd, uint32_t addr);
static bool check_address_range(uint32_t addr, uint32_t length);
static bool is_sector_aligned(uint32_t addr);
static bool is_page_aligned(uint32_t addr);

/**
 * @brief Send command with address
 */
static void send_command_addr(w25q80_t *dev, uint8_t cmd, uint32_t addr)
{
    uint8_t tx_data[4];

    tx_data[0] = cmd;
    tx_data[1] = (addr >> 16) & 0xFF;  /* Address high byte */
    tx_data[2] = (addr >> 8) & 0xFF;   /* Address middle byte */
    tx_data[3] = addr & 0xFF;          /* Address low byte */

    dev->spi->transmit(tx_data, 4);
}

/**
 * @brief Check if address range is valid
 */
static bool check_address_range(uint32_t addr, uint32_t length)
{
    /* Check for overflow */
    if ((addr + length) < addr) {
        return false;
    }

    /* Check if exceeds flash size */
    if ((addr + length) > W25Q80_FLASH_SIZE) {
        return false;
    }

    return true;
}

/**
 * @brief Check if address is sector aligned
 */
static bool is_sector_aligned(uint32_t addr)
{
    return (addr % W25Q80_SECTOR_SIZE) == 0;
}

/**
 * @brief Check if address is page aligned
 */
static bool is_page_aligned(uint32_t addr)
{
    return (addr % W25Q80_PAGE_SIZE) == 0;
}

/* ==================== Basic SPI Interface Functions ==================== */

bool w25q80_init(w25q80_t *dev, const w25q80_spi_t *spi)
{
    if (dev == NULL || spi == NULL) {
        return false;
    }

    if (spi->init == NULL || spi->select == NULL ||
        spi->deselect == NULL || spi->transfer == NULL) {
        return false;
    }

    /* Initialize SPI hardware */
    if (!spi->init()) {
        return false;
    }

    dev->spi = spi;

    /* Release from power down */
    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_RELEASE_PDOWN);
    dev->spi->deselect();

    /* Small delay for power up */
    for (volatile int i = 0; i < 1000; i++);

    /* Verify device ID */
    uint8_t manufacturer_id, device_id;
    if (!w25q80_read_id(dev, &manufacturer_id, &device_id)) {
        return false;
    }

    if (manufacturer_id != W25Q80_MANUFACTURER_ID || device_id != W25Q80_DEVICE_ID) {
        return false;
    }

    dev->initialized = true;
    return true;
}

void w25q80_deinit(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return;
    }

    /* Put flash in power down mode */
    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_POWER_DOWN);
    dev->spi->deselect();

    /* Deinitialize SPI hardware */
    if (dev->spi->deinit != NULL) {
        dev->spi->deinit();
    }

    dev->initialized = false;
    dev->spi = NULL;
}

bool w25q80_read_id(w25q80_t *dev, uint8_t *manufacturer_id, uint8_t *device_id)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    /* ʹ���������豸ID���� (0x90)
     * �����ʽ: 0x90 + 3�ֽڵ�ַ(0x000000) + ����2�ֽ�ID
     * �ܹ�����6�ֽ�: ����4�ֽ�(����+��ַ), ����2�ֽ�ID
     */
    uint8_t tx_data[6] = {W25Q80_CMD_MANUF_DEVICE_ID, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t rx_data[6] = {0};

    dev->spi->select();

    /* Send command with address and receive ID */
    if (dev->spi->transfer_full != NULL) {
        /* ����6�ֽڣ�����6�ֽ� */
        dev->spi->transfer_full(tx_data, rx_data, 6);
        /* rx_data[4] ��������ID��rx_data[5] ���豸ID */
    } else {
        /* Fallback to individual transfers: �ȷ���4�ֽڣ��ٽ���2�ֽ� */
        dev->spi->transmit(tx_data, 4);  /* ��������+3��ַ */
        dev->spi->receive(rx_data + 4, 2);  /* ����2�ֽ�ID��rx_data[4]��rx_data[5] */
    }

    dev->spi->deselect();

    if (manufacturer_id != NULL) {
        *manufacturer_id = rx_data[4];  /* ������ID�ڵ�5���ֽ� */
    }

    if (device_id != NULL) {
        *device_id = rx_data[5];  /* �豸ID�ڵ�6���ֽ� */
    }

    return true;
}

bool w25q80_read_jedec_id(w25q80_t *dev, uint8_t *manufacturer_id, uint8_t *memory_type, uint8_t *capacity_id)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    uint8_t tx_data[4] = {W25Q80_CMD_JEDEC_ID, 0x00, 0x00, 0x00};
    uint8_t rx_data[4] = {0};

    dev->spi->select();

    /* Send command and receive JEDEC ID */
    if (dev->spi->transfer_full != NULL) {
        /* ����4�ֽڣ�����4�ֽ� */
        dev->spi->transfer_full(tx_data, rx_data, 4);
        /* rx_data[0] ��������ԣ�rx_data[1]��������ID��rx_data[2]���ڴ����ͣ�rx_data[3]������ID */
    } else {
        /* Fallback to individual transfers */
        dev->spi->transmit(tx_data, 4);
        dev->spi->receive(rx_data + 1, 3);  /* ��rx_data[1]��ʼ����3�ֽ� */
    }

    dev->spi->deselect();

    if (manufacturer_id != NULL) {
        *manufacturer_id = rx_data[1];  /* ������ID�ڵڶ����ֽ� */
    }

    if (memory_type != NULL) {
        *memory_type = rx_data[2];  /* �ڴ������ڵ������ֽ� */
    }

    if (capacity_id != NULL) {
        *capacity_id = rx_data[3];  /* ����ID�ڵ��ĸ��ֽ� */
    }

    return true;
}

uint8_t w25q80_read_status1(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return 0xFF;
    }

    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_READ_STATUS1);
    uint8_t status = dev->spi->transfer(0x00);
    dev->spi->deselect();

    return status;
}

uint8_t w25q80_read_status2(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return 0xFF;
    }

    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_READ_STATUS2);
    uint8_t status = dev->spi->transfer(0x00);
    dev->spi->deselect();

    return status;
}

bool w25q80_wait_busy(w25q80_t *dev, uint32_t timeout_ms)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    uint32_t start_time = HAL_GetTick();  /* Use system tick timer */
    uint32_t elapsed_time = 0;
    uint32_t check_count = 0;


    while (elapsed_time < timeout_ms || timeout_ms == 0) {
        uint8_t status = w25q80_read_status1(dev);
        if ((status & W25Q80_STATUS_BUSY) == 0) {
            return true;
        }

        /* Small delay */
        for (volatile int i = 0; i < 100; i++);

        /* Update elapsed time */
        elapsed_time = HAL_GetTick() - start_time;
        check_count++;

        /* 每10次检查打印一次状态 */
        if (check_count % 10 == 0) {
        }
    }
    return false; /* Timeout */
}

void w25q80_write_enable(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return;
    }

    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_WRITE_ENABLE);
    dev->spi->deselect();
}

void w25q80_write_disable(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return;
    }

    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_WRITE_DISABLE);
    dev->spi->deselect();
}

bool w25q80_erase_sector(w25q80_t *dev, uint32_t sector_addr)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    if (!is_sector_aligned(sector_addr)) {
        return false;
    }

    if (!check_address_range(sector_addr, W25Q80_SECTOR_SIZE)) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    /* Enable write */
    w25q80_write_enable(dev);

    /* Send erase command */
    dev->spi->select();
    send_command_addr(dev, W25Q80_CMD_BLOCK_ERASE_4K, sector_addr);
    dev->spi->deselect();
    return true;
}

bool w25q80_erase_block_32k(w25q80_t *dev, uint32_t block_addr)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    if ((block_addr % 32768) != 0) {  /* 32KB alignment */
        return false;
    }

    if (!check_address_range(block_addr, 32768)) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    /* Enable write */
    w25q80_write_enable(dev);

    /* Send erase command */
    dev->spi->select();
    send_command_addr(dev, W25Q80_CMD_BLOCK_ERASE_32K, block_addr);
    dev->spi->deselect();

    return true;
}

bool w25q80_erase_block_64k(w25q80_t *dev, uint32_t block_addr)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    if ((block_addr % 65536) != 0) {  /* 64KB alignment */
        return false;
    }

    if (!check_address_range(block_addr, 65536)) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    /* Enable write */
    w25q80_write_enable(dev);

    /* Send erase command */
    dev->spi->select();
    send_command_addr(dev, W25Q80_CMD_BLOCK_ERASE_64K, block_addr);
    dev->spi->deselect();

    return true;
}

bool w25q80_erase_chip(w25q80_t *dev)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    /* Enable write */
    w25q80_write_enable(dev);

    /* Send chip erase command */
    dev->spi->select();
    dev->spi->transfer(W25Q80_CMD_CHIP_ERASE);
    dev->spi->deselect();

    return true;
}

bool w25q80_read(w25q80_t *dev, uint32_t addr, uint8_t *data, uint32_t length)
{
    if (dev == NULL || dev->spi == NULL || data == NULL) {
        return false;
    }

    if (length == 0) {
        return true;  /* Nothing to read */
    }

    if (!check_address_range(addr, length)) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    dev->spi->select();

    /* Send read command with address */
    send_command_addr(dev, W25Q80_CMD_READ_DATA, addr);

    /* Read data - for SPI Flash, we need to send dummy bytes (0xFF) to receive data */
    if (dev->spi->transfer_full != NULL) {
        /* Use stack-allocated buffer for small reads, heap for large reads */
        #define MAX_STACK_BUFFER 256
        if (length <= MAX_STACK_BUFFER) {
            /* Small read: use stack buffer */
            uint8_t dummy_tx[MAX_STACK_BUFFER];
            memset(dummy_tx, 0xFF, length);
            dev->spi->transfer_full(dummy_tx, data, length);
        } else {
            /* Large read: do it in chunks */
            uint32_t remaining = length;
            uint8_t *ptr = data;

            while (remaining > 0) {
                uint32_t chunk = (remaining > MAX_STACK_BUFFER) ? MAX_STACK_BUFFER : remaining;
                uint8_t dummy_tx[MAX_STACK_BUFFER];
                memset(dummy_tx, 0xFF, chunk);
                dev->spi->transfer_full(dummy_tx, ptr, chunk);
                ptr += chunk;
                remaining -= chunk;
            }
        }
    } else {
        /* For receive-only function, we need to send 0xFF for each byte */
        for (uint32_t i = 0; i < length; i++) {
            data[i] = dev->spi->transfer(0xFF);
        }
    }

    dev->spi->deselect();
    return true;
}

bool w25q80_write_page(w25q80_t *dev, uint32_t addr, const uint8_t *data, uint32_t length)
{
    if (dev == NULL || dev->spi == NULL || data == NULL) {
        return false;
    }

    if (length == 0 || length > W25Q80_PAGE_SIZE) {
        return false;
    }

    if (!is_page_aligned(addr)) {
        return false;
    }

    if (!check_address_range(addr, length)) {
        return false;
    }

    /* Wait for previous operations to complete */
    if (!w25q80_wait_busy(dev, 1000)) {
        return false;
    }

    /* Enable write */
    w25q80_write_enable(dev);

    /* Send page program command */
    dev->spi->select();
    send_command_addr(dev, W25Q80_CMD_PAGE_PROGRAM, addr);

    /* Write data */
    dev->spi->transmit(data, length);

    dev->spi->deselect();

    return true;
}

bool w25q80_write(w25q80_t *dev, uint32_t addr, const uint8_t *data, uint32_t length)
{
    if (dev == NULL || dev->spi == NULL || data == NULL) {
        return false;
    }

    if (length == 0) {
        return true;  /* Nothing to write */
    }

    if (!check_address_range(addr, length)) {
        return false;
    }

    uint32_t remaining = length;
    uint32_t current_addr = addr;
    const uint8_t *current_data = data;

    while (remaining > 0) {
        /* Calculate bytes remaining in current page */
        uint32_t page_offset = current_addr % W25Q80_PAGE_SIZE;
        uint32_t bytes_in_page = W25Q80_PAGE_SIZE - page_offset;
        uint32_t write_len = (remaining < bytes_in_page) ? remaining : bytes_in_page;

        /* Write to current page */
        if (!w25q80_write_page(dev, current_addr, current_data, write_len)) {
            return false;
        }

        /* Wait for write to complete */
        if (!w25q80_wait_busy(dev, 1000)) {
            return false;
        }

        /* Update pointers */
        current_addr += write_len;
        current_data += write_len;
        remaining -= write_len;
    }

    return true;
}

bool w25q80_is_erased(w25q80_t *dev, uint32_t addr, uint32_t length)
{
    if (dev == NULL || dev->spi == NULL) {
        return false;
    }

    if (!check_address_range(addr, length)) {
        return false;
    }

    /* Read buffer for checking */
    uint8_t buffer[256];
    uint32_t remaining = length;
    uint32_t current_addr = addr;

    while (remaining > 0) {
        uint32_t read_len = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;

        if (!w25q80_read(dev, current_addr, buffer, read_len)) {
            return false;
        }

        /* Check if all bytes are 0xFF */
        for (uint32_t i = 0; i < read_len; i++) {
            if (buffer[i] != 0xFF) {
                return false;
            }
        }

        current_addr += read_len;
        remaining -= read_len;
    }
    return true;
}

/* ==================== FATFS Disk I/O Interface ==================== */

/**
 * @brief Get driver instance for FATFS
 */
static w25q80_t* get_driver_instance(uint8_t pdrv)
{
    /* Currently only support one drive (pdrv = 0) */
    if (pdrv != 0) {
        return NULL;
    }

    return &w25q80_drv;
}

int w25q80_disk_initialize(uint8_t pdrv)
{
    w25q80_t *dev = get_driver_instance(pdrv);

    if (dev == NULL) {
        return 1;  /* NOT READY */
    }

    /* Set SPI hardware abstraction */
    dev->spi = &example_spi;

    if (dev->spi == NULL) {
        return 1;  /* NOT READY */
    }

    /* Check if already initialized */
    if (dev->initialized) {
        return 0;  /* OK */
    }

    /* Try to initialize */
    if (w25q80_init(dev, dev->spi)) {
        return 0;  /* OK */
    }
    return 1;  /* NOT READY */
}

int w25q80_disk_status(uint8_t pdrv)
{
    w25q80_t *dev = get_driver_instance(pdrv);

    if (dev == NULL || dev->spi == NULL || !dev->initialized) {
        return 1;  /* NOT READY */
    }

    /* Check if flash is busy */
    uint8_t status = w25q80_read_status1(dev);
    if (status & W25Q80_STATUS_BUSY) {
        return 1;  /* NOT READY */
    }

    return 0;  /* OK */
}

int w25q80_disk_read(uint8_t pdrv, uint8_t *buff, uint32_t sector, uint32_t count)
{
    w25q80_t *dev = get_driver_instance(pdrv);

    if (dev == NULL || dev->spi == NULL || !dev->initialized || buff == NULL) {
        return 1;  /* ERROR */
    }

    if (count == 0) {
        return 0;  /* OK - nothing to read */
    }

    /* Check sector range */
    if ((sector + count) > dev->sector_count) {
        return 3;  /* PARAMETER ERROR */
    }

    /* Calculate address and length */
    uint32_t addr = sector * dev->sector_size;
    uint32_t length = count * dev->sector_size;

    /* Read data */
    if (!w25q80_read(dev, addr, buff, length)) {
        return 1;  /* ERROR */
    }

    return 0;  /* OK */
}

int w25q80_disk_write(uint8_t pdrv, const uint8_t *buff, uint32_t sector, uint32_t count)
{
    w25q80_t *dev = get_driver_instance(pdrv);

    if (dev == NULL || dev->spi == NULL || !dev->initialized || buff == NULL) {
        return 1;  /* ERROR */
    }

    if (count == 0) {
        return 0;  /* OK - nothing to write */
    }

    /* Check sector range */
    if ((sector + count) > dev->sector_count) {
        return 3;  /* PARAMETER ERROR */
    }

    /* Calculate address and length */
    uint32_t addr = sector * dev->sector_size;
    uint32_t length = count * dev->sector_size;

    /* Check if sectors need to be erased first */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t current_sector = sector + i;
        uint32_t current_addr = current_sector * dev->sector_size;

        /* Check if sector is already erased */
        if (!w25q80_is_erased(dev, current_addr, dev->sector_size)) {
            /* Need to erase the sector first */
            if (!w25q80_erase_sector(dev, current_addr)) {
                return 1;  /* ERROR */
            }

            /* Wait for erase to complete */
            if (!w25q80_wait_busy(dev, 5000)) {  /* Longer timeout for erase */
                return 1;  /* ERROR */
            }
        }
    }

    /* Write data */
    if (!w25q80_write(dev, addr, buff, length)) {
        return 1;  /* ERROR */
    }
    return 0;  /* OK */
}

int w25q80_disk_ioctl(uint8_t pdrv, uint8_t cmd, void *buff)
{
    w25q80_t *dev = get_driver_instance(pdrv);

    if (dev == NULL || dev->spi == NULL || !dev->initialized) {
        return 1;  /* ERROR */
    }

    switch (cmd) {
        case 0:  /* CTRL_SYNC - no operation needed for flash */
            return 0;  /* OK */

        case 1:  /* GET_SECTOR_COUNT */
            if (buff != NULL) {
                *((uint32_t*)buff) = dev->sector_count;
                return 0;  /* OK */
            }
            break;

        case 2:  /* GET_SECTOR_SIZE */
            if (buff != NULL) {
                *((uint32_t*)buff) = dev->sector_size;
                return 0;  /* OK */
            }
            break;

        case 3:  /* GET_BLOCK_SIZE */
            if (buff != NULL) {
                uint32_t block_size = W25Q80_BLOCK_SIZE / dev->sector_size;
                *((uint32_t*)buff) = block_size;
                return 0;  /* OK */
            }
            break;

        case 4:  /* CTRL_TRIM - not supported for flash */
            return 2;  /* PARAMETER ERROR */

        default:
            break;
    }
    return 2;  /* PARAMETER ERROR */
}


static bool w25q80_spi_init(void)
{ 
  spi2_init();
  HAL_GPIO_WritePin(SPI2_CS_GPIO_PORT, SPI2_CS_GPIO_PIN, GPIO_PIN_SET);
	spi2_set_speed(SPI_SPEED_32);
	return true;
}

static void w25q80_spi_deinit(void)
{ 
   ;
}
static void w25q80_spi_select(void)
{
    /* User implementation:
     * 1. Set CS pin LOW
     */
    HAL_GPIO_WritePin(SPI2_CS_GPIO_PORT, SPI2_CS_GPIO_PIN, GPIO_PIN_RESET);
}

static void w25q80_spi_deselect(void)
{
    /* User implementation:
     * 1. Set CS pin HIGH
     */
    HAL_GPIO_WritePin(SPI2_CS_GPIO_PORT, SPI2_CS_GPIO_PIN, GPIO_PIN_SET);
}

static uint8_t w25q80_spi_transfer(uint8_t data)
{
    /* User implementation:
     * 1. Transmit data via SPI
     * 2. Receive data via SPI
     * 3. Return received byte
     */
    uint8_t rx_data = 0;
    rx_data = spi2_read_write_byte(data); 
    return rx_data;
}

static void w25q80_spi_transmit(const uint8_t *data, uint32_t length)
{
    /* User implementation:
     * 1. Transmit multiple bytes via SPI
     */
     spi2_write_more_byte((uint8_t*)data, length); 
}

static void w25q80_spi_receive(uint8_t *data, uint32_t length)
{
    /* User implementation:
     * 1. Receive multiple bytes via SPI
     */
     spi2_receive_more_byte(data, length); 
}

static void w25q80_spi_transfer_full(const uint8_t *tx_data, uint8_t *rx_data, uint32_t length)
{
    /* User implementation:
     * 1. Full duplex SPI transfer
     */
     spi2_tx_rx_more_byte((uint8_t*)tx_data, rx_data, length);  
}

/* SPI hardware abstraction structure */
static const w25q80_spi_t example_spi = {
    .init = w25q80_spi_init,
    .deinit = w25q80_spi_deinit,
    .select = w25q80_spi_select,
    .deselect = w25q80_spi_deselect,
    .transfer = w25q80_spi_transfer,
    .transmit = w25q80_spi_transmit,
    .receive = w25q80_spi_receive,
    .transfer_full = w25q80_spi_transfer_full
};