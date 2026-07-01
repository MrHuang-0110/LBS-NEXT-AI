#include "port_file.h"

extern void usb_printf(char *fmt, ...);
bool FS_TouchFile_And_WriteData(char *filename,uint8_t *data,uint16_t length,fm_file_type_t type)
{
   
   if (fm_write(filename, type,
                (const uint8_t*)data,length)) {
				usb_printf("write %s ok\r\n",filename);
        return true;
    } else {
			 usb_printf("write %s error\r\n",filename);
        return false;
    }
		return false;
}

uint32_t FS_ReadFileData(char*filename,uint8_t *bufer,uint16_t len,fm_file_type_t type)
{
    fm_file_handle_t handle;
    if (fm_open(filename, type, &handle)) {
 
        uint32_t bytes_read = fm_read(&handle, (uint8_t*)bufer, len);
				fm_close(&handle);
				return bytes_read;
         
    } else {
       return 0;
    }
		return 0;
}