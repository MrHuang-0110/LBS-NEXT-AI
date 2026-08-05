#ifndef __PORT_FILE_H
#define __PORT_FILE_H
#include "file_manager.h"



bool FS_TouchFile_And_WriteData(char *filename,uint8_t *data,uint16_t length,fm_file_type_t type);
uint32_t FS_ReadFileData(char*filename,uint8_t *bufer,uint16_t len,fm_file_type_t type);

#endif