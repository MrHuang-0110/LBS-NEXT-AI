#ifndef DRV_MEM_H
#define DRV_MEM_H

#include <stdint.h>

#define DRV_FILE_RAM_SIZE   (32U * 1024U)

uint8_t *DrvMem_GetFileBuffer(void);
uint32_t DrvMem_GetFileBufferSize(void);
void DrvMem_SetFileLength(uint32_t len);
uint32_t DrvMem_GetFileLength(void);
void DrvMem_ClearFile(void);

#endif
