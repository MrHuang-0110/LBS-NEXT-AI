 
#include "string.h"
#include "stdlib.h"
#include "usart.h"
#include "exfuns.h"
#include "fattester.h"
#include "ff.h" 
#include "w25q80.h" 
#include "frame.h"
#include "lbsfilemanager.h"
#include "key.h"
#include "ui_manager.h"

#define FILE_MAX_TYPE_NUM       7       
#define FILE_MAX_SUBT_NUM       7      

static FileInfo file_list[MAX_FILES];

char *const FILE_TYPE_TBL[FILE_MAX_TYPE_NUM][FILE_MAX_SUBT_NUM] =
{
    {"BIN"},            
    {"LRC"},            
    {"NES", "SMS"},     
    {"TXT", "C", "H"},  
    {"WAV", "MP3", "OGG", "FLAC", "AAC", "WMA", "MID"},  
    {"BMP", "JPG", "JPEG", "GIF"},  
    {"AVI"},           
};
    

FATFS *fs[FF_VOLUMES];  

uint8_t exfuns_init(void)
{
    uint8_t i;
    uint8_t res = 0;

    for (i = 0; i < FF_VOLUMES; i++)
    {
        fs[i] = (FATFS *)ff_memalloc(sizeof(FATFS));  

        if (!fs[i])break;
    }
    
#if USE_FATTESTER == 1 
    res = mf_init();    
#endif
    
    if (i == FF_VOLUMES && res == 0)
    {
        return 0;   
    }
    else 
    {
        return 1;
    }
}


uint8_t exfuns_char_upper(uint8_t c)
{
    if (c < 'A')return c;   

    if (c >= 'a')
    {
        return c - 0x20;    
    }
    else
    {
        return c;           
    }
}


uint8_t exfuns_file_type(char *fname)
{
    uint8_t tbuf[5];
    char *attr = 0;   
    uint8_t i = 0, j;

    while (i < 250)
    {
        i++;

        if (*fname == '\0')break;   

        fname++;
    }

    if (i == 250)return 0XFF;   

    for (i = 0; i < 5; i++)   
    {
        fname--;

        if (*fname == '.')
        {
            fname++;
            attr = fname;
            break;
        }
    }

    if (attr == 0)return 0XFF;

    strcpy((char *)tbuf, (const char *)attr);   

    for (i = 0; i < 4; i++)tbuf[i] = exfuns_char_upper(tbuf[i]);   

    for (i = 0; i < FILE_MAX_TYPE_NUM; i++)        
    {
        for (j = 0; j < FILE_MAX_SUBT_NUM; j++)    
        {
            if (*FILE_TYPE_TBL[i][j] == 0)break;    

            if (strcmp((const char *)FILE_TYPE_TBL[i][j], (const char *)tbuf) == 0) 
            {
                return (i << 4) | j;
            }
        }
    }

    return 0XFF;    
}


uint8_t exfuns_get_free(uint8_t *pdrv, uint32_t *total, uint32_t *free)
{
	#if FF_FS_MINIMIZE == 0
    FATFS *fs1;
    uint8_t res;
    uint32_t fre_clust = 0, fre_sect = 0, tot_sect = 0;
    
 
    res = (uint32_t)f_getfree((const TCHAR *)pdrv, (DWORD *)&fre_clust, &fs1);

    if (res == 0)
    {
        tot_sect = (fs1->n_fatent - 2) * fs1->csize;  
        fre_sect = fre_clust * fs1->csize;             
 #if FF_MAX_SS!=512 
       // tot_sect *= fs1->ssize / 512;
       // fre_sect *= fs1->ssize / 512;
 #endif
        *total = (tot_sect * 4096)/1024.0f;    
        *free = (fre_sect * 4096)/1024.0f;    
    }

    return res;
		#else
		return 0;
		#endif
}


uint8_t exfuns_file_copy(uint8_t(*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode), uint8_t *psrc, uint8_t *pdst, 
                                      uint32_t totsize, uint32_t cpdsize, uint8_t fwmode)
{
	#if FF_FS_MINIMIZE == 0
    uint8_t res;
    uint16_t br = 0;
    uint16_t bw = 0;
    FIL *fsrc = 0;
    FIL *fdst = 0;
    uint8_t *fbuf = 0;
    uint8_t curpct = 0;
    unsigned long long lcpdsize = cpdsize;
    
    fsrc = (FIL *)ff_memalloc(sizeof(FIL));   
    fdst = (FIL *)ff_memalloc(sizeof(FIL));
    fbuf = (uint8_t *)ff_memalloc(8192);

    if (fsrc == NULL || fdst == NULL || fbuf == NULL)
    {
        res = 100;  
    }
    else
    {
        if (fwmode == 0)
        {
            fwmode = FA_CREATE_NEW;   
        }
        else 
        {
            fwmode = FA_CREATE_ALWAYS; 
        }
        
        res = f_open(fsrc, (const TCHAR *)psrc, FA_READ | FA_OPEN_EXISTING);  

        if (res == 0)res = f_open(fdst, (const TCHAR *)pdst, FA_WRITE | fwmode);  

        if (res == 0)         
        {
            if (totsize == 0) 
            {
                totsize = fsrc->obj.objsize;
                lcpdsize = 0;
                curpct = 0;
            }
            else
            {
                curpct = (lcpdsize * 100) / totsize;           
            }
            
            fcpymsg(psrc, curpct, 0X02);                       

            while (res == 0)   
            {
                res = f_read(fsrc, fbuf, 8192, (UINT *)&br);  

                if (res || br == 0)break;

                res = f_write(fdst, fbuf, (UINT)br, (UINT *)&bw);
                lcpdsize += bw;

                if (curpct != (lcpdsize * 100) / totsize)     
                {
                    curpct = (lcpdsize * 100) / totsize;

                    if (fcpymsg(psrc, curpct, 0X02))            
                    {
                        res = 0XFF;                            
                        break;
                    }
                }

                if (res || bw < br)break;
            }

            f_close(fsrc);
            f_close(fdst);
        }
    }

    ff_memfree(fsrc);
    ff_memfree(fdst);
    ff_memfree(fbuf);
    return res;
	#else
	return 0;
	#endif
}

int get_root_files_list(void)
{
	  int file_count = 0;
   
    FILINFO fno;
	  DIR dir;   
    f_opendir(&dir, "");
    
    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if(file_count >= MAX_FILES) break;
        strncpy(file_list[file_count].name, 
                fno.fname, 
                sizeof(file_list[file_count].name) - 1);
        
        file_list[file_count].size = fno.fsize;
        file_list[file_count].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
        
        file_count++;
    }
    
    f_closedir(&dir);
    return file_count;  // 返回找到的文件数量
}

bool cpy_fileName(uint8_t count,char *name)
{ 
    if(count >= MAX_FILES)return false;
	  
	  strcpy(name,file_list[count].name);
	  return true;
}

uint8_t *exfuns_get_src_dname(uint8_t *pname)
{
	
    uint16_t temp = 0;

    while (*pname != 0)
    {
        pname++;
        temp++;
    }

    if (temp < 4)return 0;

    while ((*pname != 0x5c) && (*pname != 0x2f))pname--;   

    return ++pname;
}


uint32_t exfuns_get_folder_size(uint8_t *fdname)
{
	#if FF_FS_MINIMIZE == 0
#define MAX_PATHNAME_DEPTH  512 + 1    
    uint8_t res = 0;
    DIR *fddir = 0;        
    FILINFO *finfo = 0;    
    uint8_t *pathname = 0;  
    uint16_t pathlen = 0;  
    uint32_t fdsize = 0;

    fddir = (DIR *)ff_memalloc(sizeof(DIR));  
    finfo = (FILINFO *)ff_memalloc(sizeof(FILINFO));

    if (fddir == NULL || finfo == NULL)res = 100;

    if (res == 0)
    {
        pathname = ff_memalloc(MAX_PATHNAME_DEPTH);

        if (pathname == NULL)res = 101;

        if (res == 0)
        {
            pathname[0] = 0;
            strcat((char *)pathname, (const char *)fdname);   
            res = f_opendir(fddir, (const TCHAR *)fdname);     

            if (res == 0)  
            {
                while (res == 0) 
                {
                    res = f_readdir(fddir, finfo);                 

                    if (res != FR_OK || finfo->fname[0] == 0)break; 

                    if (finfo->fname[0] == '.')continue;           

                    if (finfo->fattrib & 0X10)  
                    {
                        pathlen = strlen((const char *)pathname);  
                        strcat((char *)pathname, (const char *)"/");
                        strcat((char *)pathname, (const char *)finfo->fname);  
                        //printf("\r\nsub folder:%s\r\n",pathname);     
                        fdsize += exfuns_get_folder_size(pathname);    
                        pathname[pathlen] = 0;                         
                    }
                    else
                    {
                        fdsize += finfo->fsize; 
                    }
                }
            }

            ff_memfree(pathname);
        }
    }

    ff_memfree(fddir);
    ff_memfree(finfo);

    if (res)
    {
        return 0;
    }
    else 
    {
        return fdsize;
    }
	#else
	return 0;
	#endif
}


uint8_t exfuns_folder_copy(uint8_t(*fcpymsg)(uint8_t *pname, uint8_t pct, uint8_t mode), uint8_t *psrc, uint8_t *pdst, 
                           uint32_t *totsize, uint32_t *cpdsize, uint8_t fwmode)
{
#if FF_FS_MINIMIZE == 0
#define MAX_PATHNAME_DEPTH 512 + 1  
    uint8_t res = 0;
    DIR *srcdir = 0;    
    DIR *dstdir = 0;    
    FILINFO *finfo = 0; 
    uint8_t *fn = 0;    

    uint8_t *dstpathname = 0;   
    uint8_t *srcpathname = 0; 

    uint16_t dstpathlen = 0;   
    uint16_t srcpathlen = 0;    


    srcdir = (DIR *)ff_memalloc(sizeof(DIR));  
    dstdir = (DIR *)ff_memalloc(sizeof(DIR));
    finfo = (FILINFO *)ff_memalloc(sizeof(FILINFO));

    if (srcdir == NULL || dstdir == NULL || finfo == NULL)res = 100;

    if (res == 0)
    {
        dstpathname = ff_memalloc(MAX_PATHNAME_DEPTH);
        srcpathname = ff_memalloc(MAX_PATHNAME_DEPTH);

        if (dstpathname == NULL || srcpathname == NULL)res = 101;

        if (res == 0)
        {
            dstpathname[0] = 0;
            srcpathname[0] = 0;
            strcat((char *)srcpathname, (const char *)psrc); 
            strcat((char *)dstpathname, (const char *)pdst);  
            res = f_opendir(srcdir, (const TCHAR *)psrc);      

            if (res == 0)  
            {
                strcat((char *)dstpathname, (const char *)"/"); 
                fn = exfuns_get_src_dname(psrc);

                if (fn == 0)   
                {
                    dstpathlen = strlen((const char *)dstpathname);
                    dstpathname[dstpathlen] = psrc[0]; 
                    dstpathname[dstpathlen + 1] = 0;  
                }
                else strcat((char *)dstpathname, (const char *)fn); 

                fcpymsg(fn, 0, 0X04); 
                res = f_mkdir((const TCHAR *)dstpathname);  

                if (res == FR_EXIST)res = 0;

                while (res == 0) 
                {
                    res = f_readdir(srcdir, finfo);  

                    if (res != FR_OK || finfo->fname[0] == 0)break;

                    if (finfo->fname[0] == '.')continue;  

                    fn = (uint8_t *)finfo->fname;         
                    dstpathlen = strlen((const char *)dstpathname); 
                    srcpathlen = strlen((const char *)srcpathname);

                    strcat((char *)srcpathname, (const char *)"/");

                    if (finfo->fattrib & 0X10) 
                    {
                        strcat((char *)srcpathname, (const char *)fn);
                        res = exfuns_folder_copy(fcpymsg, srcpathname, dstpathname, totsize, cpdsize, fwmode);   
                    }
                    else    
                    {
                        strcat((char *)dstpathname, (const char *)"/"); 
                        strcat((char *)dstpathname, (const char *)fn); 
                        strcat((char *)srcpathname, (const char *)fn);  
                        fcpymsg(fn, 0, 0X01);      
                        res = exfuns_file_copy(fcpymsg, srcpathname, dstpathname, *totsize, *cpdsize, fwmode); 
                        *cpdsize += finfo->fsize;  
                    }

                    srcpathname[srcpathlen] = 0;   
                    dstpathname[dstpathlen] = 0;   
                }
            }

            ff_memfree(dstpathname);
            ff_memfree(srcpathname);
        }
    }

    ff_memfree(srcdir);
    ff_memfree(dstdir);
    ff_memfree(finfo);
    return res;
#else
return 0;
#endif
}


FRESULT exfuns_fatfs_init(void)
{
    FRESULT res;

    if (exfuns_init() != 0) {
        return FR_INT_ERR;
    }

    res = f_mount(fs[0], "0:", 1);

    if (res == FR_NO_FILESYSTEM)
    {
			  #if 0
        w25q80_disk_initialize(0);

        if (w25q80_erase_chip(&w25q80_drv)) {
            w25q80_wait_busy(&w25q80_drv, 15000);
        }

        res = fatfs_format("0:");
        if (res != FR_OK) {
            return res; 
        }

        res = f_mount(fs[0], "0:", 0);
        if (res != FR_OK) {
            return res;
        }
			  #endif
			  return FR_DISK_ERR;
    }
    else if (res != FR_OK)
    {
        return res;
    }


    return FR_OK;
}
FRESULT fatfs_create_file(char *path,const void *data,uint32_t length)
{ 
    FRESULT res;
	  FIL fsrc;
	  UINT bw;
	  res = f_open(&fsrc, (const TCHAR *)path, FA_WRITE | FA_CREATE_ALWAYS); 
    if(res!=FR_OK){f_close(&fsrc);return res;} 
	  
	  res = f_write(&fsrc, data,length,&bw);
	  if(res != FR_OK || (bw != length)){f_close(&fsrc);return res;} 
	
	  f_close(&fsrc);
	  return FR_OK;
}
uint32_t get_file_length(char *path)
{ 
    FRESULT res;
	  FIL fsrc;
	  uint32_t length;
	  res = f_open(&fsrc, (const TCHAR *)path, FA_READ); 
    if(res!=FR_OK){f_close(&fsrc);return res;}
    length = fsrc.obj.objsize;		
	  f_close(&fsrc);
		
	  return length;    
}

FRESULT fatfs_read_file(char *path,void *data,uint32_t length)
{ 
    FRESULT res;
	  FIL fsrc;
	  UINT bw;
	  res = f_open(&fsrc, (const TCHAR *)path, FA_READ); 
    if(res!=FR_OK){f_close(&fsrc);return res;} 
	  
	  res = f_read(&fsrc,data,length,&bw);
	  if(res != FR_OK || (bw != length)){f_close(&fsrc);return res;} 
	  f_close(&fsrc);
	  return FR_OK;
}

FRESULT fatfs_format(const TCHAR* path)
{
    FRESULT res;
    MKFS_PARM fmt_opt;
    uint8_t *work_buf;

    work_buf = ff_memalloc(FF_MAX_SS);
    if (!work_buf) {
        return FR_NOT_ENOUGH_CORE;
    }

    memset(&fmt_opt, 0, sizeof(MKFS_PARM));
    fmt_opt.fmt = FM_FAT | FM_SFD;
    res = f_mkfs(path, &fmt_opt, work_buf, FF_MAX_SS);
    ff_memfree(work_buf);

    if (res == FR_OK) {
        return FR_OK;
    }

    work_buf = ff_memalloc(FF_MAX_SS);
    if (!work_buf) {
        return FR_NOT_ENOUGH_CORE;
    }

    memset(&fmt_opt, 0, sizeof(MKFS_PARM));
    fmt_opt.fmt = FM_SFD;
    res = f_mkfs(path, &fmt_opt, work_buf, FF_MAX_SS);
    ff_memfree(work_buf);

    if (res == FR_OK) {
        return FR_OK;
    }

    work_buf = ff_memalloc(FF_MAX_SS);
    if (!work_buf) {
        return FR_NOT_ENOUGH_CORE;
    }

    memset(&fmt_opt, 0, sizeof(MKFS_PARM));
    fmt_opt.fmt = FM_ANY; 
    res = f_mkfs(path, &fmt_opt, work_buf, FF_MAX_SS);
    ff_memfree(work_buf);

    return res; 
}
 
void touchOtherFile(uint8_t index, uint8_t *data, uint16_t length,void (*port_transerf_data)(void *data,uint16_t length))
{ 
    uint8_t response[10];
	  static char name[16]; 
	  static uint8_t  receive_data[256];
	  static bool is_file = false;
	  static FIL fsrc;
	  UINT BW;
	  FRESULT res;
	
    switch(index) {
        case 0xDA: { 
					char *end;
					long fileId = strtol((const char*)data, &end, 10);
						if(end == (const char *)fileId){ 
							response[0] = 0x01;
						  goto _FILE_ERROR;
						}					
						memset(name,0,sizeof(name));
						strncpy(name,(const char*)data,length);         
					  memset((FIL*)&fsrc,0,sizeof(FIL));
					  res = f_open(&fsrc,(char*)name,FA_WRITE|FA_CREATE_ALWAYS);
					  if(res!=FR_OK)
						{
						   response[0] = res;	
							 goto _FILE_ERROR;
						}
						is_file = true;		
            response[0] = 0x01;		
						extern void set_event_disable(char*name);
//            set_event_disable("monitor_event");						
						MultiUart_SendFrame(port_transerf_data, response, 1, 0xFD);
            break;
        }
        
        case 0xAA:
				case 0xBB:
				case 0xBC:{
					
            if(!is_file) {
                response[0] = 0xF3;  
                goto _FILE_ERROR;
            }
						res = f_write(&fsrc,(uint8_t*)data,length,&BW);
						if(BW!=length || res!=FR_OK)
						{ 
                response[0] = 0xF4;  
                goto _FILE_ERROR;						    
						}
						if(0xBB == index || 0xBC == index)
						{ 							 				   
               is_file = false;
							 f_close(&fsrc);
							 ui_manager_add_name(name);
							 touchFileOKCallBack();
							 if(index == 0xBC)
							 { 				 
							    /*run python*/
									extern volatile bool start_py;
									start_py = true;		
									set_entery_short(1);								    
							 }							
 
						 is_file = false;	
						}			
		         response[0] = 0x01;		
						 MultiUart_SendFrame(port_transerf_data, response, 1, 0xFD);
						 extern void set_event_enable(char*name);
//						 set_event_enable("monitor_event");
            break;
        }
        
        default: {
            response[0] = 0xFF; 
            goto _FILE_ERROR;
        }
    }  
		 	 
		return;
		_FILE_ERROR:	   
		  is_file = false;
		  f_close(&fsrc);
		  f_unlink(name);
			MultiUart_SendFrame(port_transerf_data, response, 1, response[0]);	
		  touchFileErrorCallBack();	 
			 extern void set_event_enable(char*name);
//	    set_event_enable("monitor_event");
}

 










