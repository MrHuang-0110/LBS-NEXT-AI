/**
 ****************************************************************************************************
 * @file        fattester.c
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.1
 * @date        2022-10-28
 * @brief       FATFS ���Դ���
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� STM32������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20200403
 * ��һ�η���
 * V1.1 20221028
 * ����FATFS R0.14b
 ****************************************************************************************************
 */

#include "string.h"
#include "stdlib.h"
#include "usart.h"
#include "exfuns.h"
#include "fattester.h"
#include "w25q80.h"

/* ���֧���ļ�ϵͳ���� ,��ʹ�����´��� */
#if USE_FATTESTER == 1

/* FATFS���Խṹ��
 * ��Ҫ�������ļ�ָ��/�ļ���Ϣ/Ŀ¼/��д�������Ϣ, ����fattester.c
 * ����Ĳ��Ժ���ʹ��. ������Ҫʹ���ļ�ϵͳ���Թ���ʱ
 */
_m_fattester fattester;



/**
 * @brief       ��ʼ���ļ�ϵͳ����(�����ڴ�)
 *   @note      �ú���������ִ���κ��ļ�ϵͳ����֮ǰ������һ��.
 *              �ú���ֻ��Ҫ���ɹ�����һ�μ���,�����ظ�����!!
 * @param       ��
 * @retval      ִ�н��: 0, �ɹ�; 1, ʧ��;
 */
uint8_t mf_init(void)
{
    fattester.file = (FIL *)mymalloc(SRAMIN, sizeof(FIL));      /* Ϊfile�����ڴ� */
    fattester.fatbuf = (uint8_t *)mymalloc(SRAMIN, 512);        /* Ϊfattester.fatbuf�����ڴ� */

    if (fattester.file && fattester.fatbuf)
    {
        return 0;   /* ����ɹ� */
    }
    else
    {
        mf_free();  /* �ͷ��ڴ� */
        return 1;   /* ����ʧ�� */
    }
}

/**
 * @brief       �ͷ��ļ�ϵͳ����������ڴ�
 *   @note      ������ú����Ժ�, �ļ�ϵͳ���Թ��ܽ�ʧЧ.
 * @param       ��
 * @retval      ��
 */
void mf_free(void)
{
    myfree(SRAMIN, fattester.file);
    myfree(SRAMIN, fattester.fatbuf);
}

/**
 * @brief       Ϊ����ע�Ṥ����
 * @param       path : ����·��������"0:"��"1:"
 * @param       mt   : 0��������ע��(�Ժ�ע��); 1������ע��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_mount(uint8_t *path, uint8_t mt)
{
    return f_mount(fs[1], (const TCHAR *)path, mt);
}

/**
 * @brief       ���ļ�
 * @param       path : ·�� + �ļ���
 * @param       mode : ��ģʽ
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_open(uint8_t *path, uint8_t mode)
{
    uint8_t res;
    res = f_open(fattester.file, (const TCHAR *)path, mode);    /* ���ļ� */
    return res;
}

/**
 * @brief       �ر��ļ�
 * @param       ��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_close(void)
{
    f_close(fattester.file);
    return 0;
}

/**
 * @brief       ��������
 * @param       len : �����ĳ���
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_read(uint16_t len)
{
    uint16_t i, t;
    uint8_t res = 0;
    uint16_t tlen = 0;
    uint32_t br = 0;
    printf("\r\nRead fattester.file data is:\r\n");

    for (i = 0; i < len / 512; i++)
    {
        res = f_read(fattester.file, fattester.fatbuf, 512, &br);

        if (res)
        {
            printf("Read Error:%d\r\n", res);
            break;
        }
        else
        {
            tlen += br;

            for (t = 0; t < br; t++)printf("%c", fattester.fatbuf[t]);
        }
    }

    if (len % 512)
    {
        res = f_read(fattester.file, fattester.fatbuf, len % 512, &br);

        if (res)    /* �����ݳ����� */
        {
            printf("\r\nRead Error:%d\r\n", res);
        }
        else
        {
            tlen += br;

            for (t = 0; t < br; t++)printf("%c", fattester.fatbuf[t]);
        }
    }

    if (tlen)printf("\r\nReaded data len:%d\r\n", tlen);    /* ���������ݳ��� */

    printf("Read data over\r\n");
    return res;
}

/**
 * @brief       д������
 * @param       pdata : ���ݻ�����
 * @param       len   : д�볤��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_write(uint8_t *pdata, uint16_t len)
{
    uint8_t res;
    uint32_t bw = 0;

    printf("\r\nBegin Write fattester.file...\r\n");
    printf("Write data len:%d\r\n", len);
    res = f_write(fattester.file, pdata, len, &bw);

    if (res)
    {
        printf("Write Error:%d\r\n", res);
    }
    else
    {
        printf("Writed data len:%d\r\n", bw);
    }

    printf("Write data over.\r\n");
    return res;
}

/**
 * @brief       ��Ŀ¼
 * @param       path : ·��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_opendir(uint8_t *path)
{
    return f_opendir(&fattester.dir, (const TCHAR *)path);
}

/**
 * @brief       �ر�Ŀ¼
 * @param       ��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_closedir(void)
{
    return f_closedir(&fattester.dir);
}

/**
 * @brief       ���ȡ�ļ���
 * @param       ��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_readdir(void)
{
    uint8_t res;
    res = f_readdir(&fattester.dir, &fattester.fileinfo);   /* ��ȡһ���ļ�����Ϣ */

    if (res != FR_OK)return res;    /* ������ */

    printf("\r\n fattester.dir info:\r\n");

    printf("fattester.dir.dptr:%d\r\n", fattester.dir.dptr);
    printf("fattester.dir.obj.id:%d\r\n", fattester.dir.obj.id);
    printf("fattester.dir.obj.sclust:%d\r\n", fattester.dir.obj.sclust);
    printf("fattester.dir.obj.objsize:%lld\r\n", fattester.dir.obj.objsize);
    printf("fattester.dir.obj.c_ofs:%d\r\n", fattester.dir.obj.c_ofs);
    printf("fattester.dir.clust:%d\r\n", fattester.dir.clust);
    printf("fattester.dir.sect:%d\r\n", fattester.dir.sect);
    printf("fattester.dir.blk_ofs:%d\r\n", fattester.dir.blk_ofs);

    printf("\r\n");
    printf("fattester.file Name is:%s\r\n", fattester.fileinfo.fname);
    printf("fattester.file Size is:%lld\r\n", fattester.fileinfo.fsize);
    printf("fattester.file data is:%d\r\n", fattester.fileinfo.fdate);
    printf("fattester.file time is:%d\r\n", fattester.fileinfo.ftime);
    printf("fattester.file Attr is:%d\r\n", fattester.fileinfo.fattrib);
    printf("\r\n");
    return 0;
}

/**
 * @brief       �����ļ�
 * @param       path : ·��
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_scan_files(uint8_t *path)
{
    FRESULT res;
    res = f_opendir(&fattester.dir, (const TCHAR *)path); /* ��һ��Ŀ¼ */

    if (res == FR_OK)
    {
        printf("\r\n");

        while (1)
        {
            res = f_readdir(&fattester.dir, &fattester.fileinfo);   /* ��ȡĿ¼�µ�һ���ļ� */

            if (res != FR_OK || fattester.fileinfo.fname[0] == 0)
            {
                break;  /* ������/��ĩβ��,�˳� */
            }
            
            // if (fattester.fileinfo.fname[0] == '.') continue;    /* �����ϼ�Ŀ¼ */
            printf("%s/", path);    /* ��ӡ·�� */
            printf("%s\r\n", fattester.fileinfo.fname); /* ��ӡ�ļ��� */
        }
    }

    return res;
}

/**
 * @brief       ��ʾʣ������
 * @param       path : ·��(�̷�)
 * @retval      ʣ������(�ֽ�)
 */
uint32_t mf_showfree(uint8_t *path)
{
    FATFS *fs1;
    uint8_t res;
    uint32_t fre_clust = 0, fre_sect = 0, tot_sect = 0;
    /* �õ�������Ϣ�����д����� */
    res = f_getfree((const TCHAR *)path, (DWORD *)&fre_clust, &fs1);

    if (res == 0)
    {
        tot_sect = (fs1->n_fatent - 2) * fs1->csize;/* �õ��������� */
        fre_sect = fre_clust * fs1->csize;          /* �õ����������� */
#if FF_MAX_SS!=512
        tot_sect *= fs1->ssize / 512;
        fre_sect *= fs1->ssize / 512;
#endif

        if (tot_sect < 20480)   /* ������С��10M */
        {
            /* Print free space in unit of KB (assuming 512 bytes/sector) */
            printf("\r\n����������:%d KB\r\n"
                   "���ÿռ�:%d KB\r\n",
                   tot_sect >> 1, fre_sect >> 1);
        }
        else
        {
            /* Print free space in unit of KB (assuming 512 bytes/sector) */
            printf("\r\n����������:%d MB\r\n"
                   "���ÿռ�:%d MB\r\n",
                   tot_sect >> 11, fre_sect >> 11);
        }
    }

    return fre_sect;
}

/**
 * @brief       �ļ���дָ��ƫ��
 * @param       offset : ����׵�ַ��ƫ����
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_lseek(uint32_t offset)
{
    return f_lseek(fattester.file, offset);
}

/**
 * @brief       ��ȡ�ļ���ǰ��дָ���λ��
 * @param       ��
 * @retval      ��ǰλ��
 */
uint32_t mf_tell(void)
{
    return f_tell(fattester.file);
}

/**
 * @brief       ��ȡ�ļ���С
 * @param       ��
 * @retval      �ļ���С
 */
uint32_t mf_size(void)
{
    return f_size(fattester.file);
}

/**
 * @brief       ����Ŀ¼
 * @param       path : Ŀ¼·�� + ����
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_mkdir(uint8_t *path)
{
    return f_mkdir((const TCHAR *)path);
}

/**
 * @brief       ��ʽ��
 * @param       path : ����·��������"0:"��"1:"
 * @param       opt  : ģʽ,FM_FAT,FM_FAT32,FM_EXFAT,FM_ANY��...
 * @param       au   : �ش�С
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_fmkfs(uint8_t *path, uint8_t opt, uint16_t au)
{
    MKFS_PARM temp = {FM_ANY, 0, 0, 0, 0};
    uint8_t *work_buf;
    uint8_t res;

    temp.fmt = opt;     /* �ļ�ϵͳ��ʽ,1��FM_FAT;2,FM_FAT32;4,FM_EXFAT; */
    temp.au_size = au;  /* �ش�С����,0��ʹ��Ĭ�ϴش�С */

    work_buf = ff_memalloc(FF_MAX_SS);
    if (work_buf) {
        res = f_mkfs((const TCHAR *)path, &temp, work_buf, FF_MAX_SS);    /* ��ʽ��,Ĭ�ϲ���,workbuf,����_MAX_SS��С */
        ff_memfree(work_buf);
    } else {
        res = FR_NOT_ENOUGH_CORE;
    }
    return res;
}

/**
 * @brief       ɾ���ļ�/Ŀ¼
 * @param       path : �ļ�/Ŀ¼·��+����
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_unlink(uint8_t *path)
{
    return  f_unlink((const TCHAR *)path);
}

/**
 * @brief       �޸��ļ�/Ŀ¼����(���Ŀ¼��ͬ,�������ƶ��ļ�Ŷ!)
 * @param       oldname : ֮ǰ������
 * @param       newname : ������
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_rename(uint8_t *oldname, uint8_t *newname)
{
    return  f_rename((const TCHAR *)oldname, (const TCHAR *)newname);
}

/**
 * @brief       ��ȡ�̷�(��������)
 * @param       path : ����·��������"0:"��"1:"
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
void mf_getlabel(uint8_t *path)
{
    uint8_t buf[20];
    uint32_t sn = 0;
    uint8_t res;
    res = f_getlabel ((const TCHAR *)path, (TCHAR *)buf, (DWORD *)&sn);

    if (res == FR_OK)
    {
        printf("\r\n����%s ���̷�Ϊ:%s\r\n", path, buf);
        printf("����%s �����к�:%X\r\n\r\n", path, sn);
    }
    else
    {
        printf("\r\n��ȡʧ�ܣ�������:%X\r\n", res);
    }
}

/**
 * @brief       �����̷����������֣����11���ַ�������֧�����ֺʹ�д��ĸ����Լ����ֵ�
 * @param       path : ���̺�+���֣�����"0:ALIENTEK"��"1:OPENEDV"
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
void mf_setlabel(uint8_t *path)
{
    uint8_t res;
    res = f_setlabel ((const TCHAR *)path);

    if (res == FR_OK)
    {
        printf("\r\n�����̷����óɹ�:%s\r\n", path);
    }
    else printf("\r\n�����̷�����ʧ�ܣ�������:%X\r\n", res);
}

/**
 * @brief       ���ļ������ȡһ���ַ���
 * @param       size : Ҫ��ȡ�ĳ���
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
void mf_gets(uint16_t size)
{
    TCHAR *rbuf;
    rbuf = f_gets((TCHAR *)fattester.fatbuf, size, fattester.file);

    if (*rbuf == 0)return  ; /* û�����ݶ��� */
    else
    {
        printf("\r\nThe String Readed Is:%s\r\n", rbuf);
    }
}

/**
 * @brief       дһ���ַ����ļ�(��Ҫ FF_USE_STRFUNC >= 1)
 * @param       c : Ҫд����ַ�
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_putc(uint8_t c)
{
    return f_putc((TCHAR)c, fattester.file);
}

/**
 * @brief       д�ַ������ļ�(��Ҫ FF_USE_STRFUNC >= 1)
 * @param       str : Ҫд����ַ���
 * @retval      ִ�н��(�μ�FATFS, FRESULT�Ķ���)
 */
uint8_t mf_puts(uint8_t *str)
{
    return f_puts((TCHAR *)str, fattester.file);
}

#endif












