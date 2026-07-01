/**
 ****************************************************************************************************
 * @file        diskio.c
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2022-12-16
 * @brief       FATFS�ײ�(diskio) ��������
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� STM32F103������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20221216
 * ��һ�η���
 *
 ****************************************************************************************************
 */


#include "diskio.h"
#include "w25q80.h"



#define EX_FLASH    0       /* �ⲿspi flash,����Ϊ0 */
 
/**
 * @brief       ��ô���״̬
 * @param       pdrv : ���̱��0~9
 * @retval      DSTATUS:FATFS�涨�ķ���ֵ
 */
DSTATUS disk_status (
    BYTE pdrv       /* Physical drive nmuber to identify the drive */
)
{
    return RES_OK;
}

/**
 * @brief       ��ʼ������
 * @param       pdrv : ���̱��0~9
 * @retval      DSTATUS:FATFS�涨�ķ���ֵ
 */
DSTATUS disk_initialize (
    BYTE pdrv       /* Physical drive nmuber to identify the drive */
)
{
    uint8_t res = 0;

    switch (pdrv)
    {
        case EX_FLASH:          /* �ⲿflash */
            
				    res = w25q80_disk_initialize(EX_FLASH);
            break;

        default:
            res = 1;
    }

    if (res)
    {
        return  STA_NOINIT;
    }
    else
    {
        return 0; /* ��ʼ���ɹ�*/
    }
}

/**
 * @brief       ������
 * @param       pdrv   : ���̱��0~9
 * @param       buff   : ���ݽ��ջ����׵�ַ
 * @param       sector : ������ַ
 * @param       count  : ��Ҫ��ȡ��������
 * @retval      DRESULT:FATFS�涨�ķ���ֵ
 */
DRESULT disk_read (
    BYTE pdrv,      /* Physical drive nmuber to identify the drive */
    BYTE *buff,     /* Data buffer to store read data */
    DWORD sector,   /* Sector address in LBA */
    UINT count      /* Number of sectors to read */
)
{
    uint8_t res = 0;

    if (!count) return RES_PARERR;      /* count���ܵ���0�����򷵻ز������� */

    switch (pdrv)
    {
 
          case EX_FLASH:      /* �ⲿflash */
					  res = w25q80_disk_read(EX_FLASH,buff,sector,count);
          break;

        default:
            res = 1;
    }

    /* ��������ֵ��������ֵת��ff.c�ķ���ֵ */
    if (res == 0x00)
    {
        return RES_OK;
    }
    else
    {
        return RES_ERROR; 
    }
}

/**
 * @brief       д����
 * @param       pdrv   : ���̱��0~9
 * @param       buff   : �������ݻ������׵�ַ
 * @param       sector : ������ַ
 * @param       count  : ��Ҫд���������
 * @retval      DRESULT:FATFS�涨�ķ���ֵ
 */
DRESULT disk_write (
    BYTE pdrv,          /* Physical drive nmuber to identify the drive */
    const BYTE *buff,   /* Data to be written */
    DWORD sector,       /* Sector address in LBA */
    UINT count          /* Number of sectors to write */
)
{
    uint8_t res = 0;

    if (!count) return RES_PARERR;  /* count���ܵ���0�����򷵻ز������� */

    switch (pdrv)
    {
 
        case EX_FLASH:      /* �ⲿflash */
            res = w25q80_disk_write(EX_FLASH,buff,sector,count);
            break;

        default:
            res = 1;
    }

    /* ��������ֵ��������ֵת��ff.c�ķ���ֵ */
    if (res == 0x00)
    {
        return RES_OK;
    }
    else
    {
        return RES_ERROR; 
    }
}

/**
 * @brief       ��ȡ�������Ʋ���
 * @param       pdrv   : ���̱��0~9
 * @param       ctrl   : ���ƴ���
 * @param       buff   : ����/���ջ�����ָ��
 * @retval      DRESULT:FATFS�涨�ķ���ֵ
 */
DRESULT disk_ioctl (
    BYTE pdrv,      /* Physical drive nmuber (0..) */
    BYTE cmd,       /* Control code */
    void *buff      /* Buffer to send/receive control data */
)
{
    DRESULT res = RES_ERROR; /* Ĭ������ */
    int ret;

    if (pdrv == EX_FLASH)  /* �ⲿFLASH */
    {
        ret = w25q80_disk_ioctl(EX_FLASH, cmd, buff);
        /* ��w25q80����ֵת��ΪFATFS����ֵ */
        switch (ret) {
            case 0:  /* OK */
                res = RES_OK;
                break;
            case 1:  /* ERROR */
                res = RES_ERROR;
                break;
            case 2:  /* PARAMETER ERROR */
            case 3:  /* PARAMETER ERROR (sector range) */
                res = RES_PARERR;
                break;
            default:
                res = RES_ERROR;
                break;
        }
    }
    else
    {
        res = RES_ERROR;    /* �����Ĳ�֧�� */
    }

    return res;
}




















