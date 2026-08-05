#include "drv_mem.h"

static uint8_t s_file_ram[DRV_FILE_RAM_SIZE];
static uint32_t s_file_len;

uint8_t *DrvMem_GetFileBuffer(void)
{
    return s_file_ram;
}

uint32_t DrvMem_GetFileBufferSize(void)
{
    return DRV_FILE_RAM_SIZE;
}

void DrvMem_SetFileLength(uint32_t len)
{
    if (len > DRV_FILE_RAM_SIZE)
    {
        len = DRV_FILE_RAM_SIZE;
    }
    s_file_len = len;
}

uint32_t DrvMem_GetFileLength(void)
{
    return s_file_len;
}

void DrvMem_ClearFile(void)
{
    s_file_len = 0U;
}
