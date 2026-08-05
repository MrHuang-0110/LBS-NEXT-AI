 

#ifndef __EXFUNS_H
#define __EXFUNS_H

#include "sys.h"
#include "ff.h"
#include "stdbool.h"

extern FATFS *fs[FF_VOLUMES];
extern FIL *file;
extern FIL *ftemp;
extern UINT br, bw;
extern FILINFO fileinfo;
extern DIR dir;
extern uint8_t *fatbuf;    

#define MAX_FILES   20
#define T_BIN       0X00    
#define T_LRC       0X10   

#define T_NES       0X20    
#define T_SMS       0X21   

#define T_TEXT      0X30    
#define T_C         0X31    
#define T_H         0X32    

#define T_WAV       0X40    
#define T_MP3       0X41    
#define T_OGG       0X42    
#define T_FLAC      0X43    
#define T_AAC       0X44   
#define T_WMA       0X45    
#define T_MID       0X46    

#define T_BMP       0X50    
#define T_JPG       0X51    
#define T_JPEG      0X52    
#define T_GIF       0X53    

#define T_AVI       0X60    


typedef struct {
    char name[32];
    uint32_t size;
    uint8_t is_dir;
} FileInfo;

FRESULT exfuns_fatfs_init(void);         
FRESULT fatfs_format(const TCHAR* path); 
uint8_t exfuns_init(void);                
uint8_t exfuns_file_type(char *fname);  

uint8_t exfuns_get_free(uint8_t *pdrv, uint32_t *total, uint32_t *free);   
uint32_t exfuns_get_folder_size(uint8_t *fdname);  
uint8_t *exfuns_get_src_dname(uint8_t *dpfn);
uint8_t exfuns_file_copy(uint8_t(*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode), uint8_t *psrc, uint8_t *pdst, uint32_t totsize, uint32_t cpdsize, uint8_t fwmode);       /* �ļ����� */
uint8_t exfuns_folder_copy(uint8_t(*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode), uint8_t *psrc, uint8_t *pdst, uint32_t *totsize, uint32_t *cpdsize, uint8_t fwmode);   /* �ļ��и��� */
void touchOtherFile(uint8_t index, uint8_t *data, uint16_t length,void (*port_transerf_data)(void *data,uint16_t length));
FRESULT fatfs_read_file(char *path,void *data,uint32_t length);
FRESULT fatfs_create_file(char *path,const void *data,uint32_t length);
uint32_t get_file_length(char *path);
int get_root_files_list(void);
bool cpy_fileName(uint8_t count,char *name);
#endif
























