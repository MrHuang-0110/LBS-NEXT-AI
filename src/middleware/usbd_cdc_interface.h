/**
 ****************************************************************************************************
 * @file        usbd_cdc_interface.h
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-06
 * @brief       USB VCP ��������
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
 * V1.0 20200406
 * ��һ�η���
 *
 ****************************************************************************************************
 */

#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H
#include "frame.h"
#include "usbd_cdc.h"

#define USB_CDC_DEVICE_ID 		  10
#define USB_USART_REC_LEN       600     /* USB���ڽ��ջ���������ֽ��� */

/* ��ѯ���ڣ����65ms����С1ms */
#define CDC_POLLING_INTERVAL    1       /* ��ѯ���ڣ����65ms����С1ms */

typedef struct{ 
	
 uint8_t g_user_usb_rx_buffer[USB_USART_REC_LEN]; 
 uint32_t g_user_usb_rx_len,g_sys_usb_rx_len;
 FrameParser usb_parser;
}USB_MESSAGE_BOX;

extern USBD_CDC_ItfTypeDef  USBD_CDC_fops;
extern USB_MESSAGE_BOX usb_message;
extern uint8_t g_usb_usart_printf_buffer[USB_USART_REC_LEN];
void cdc_vcp_data_tx(void *data, uint16_t Len);
void cdc_vcp_data_rx(uint8_t* buf, uint32_t len);

 
void usb_printf(char* fmt,...); 
void usb_cdc_init(void);
void reset_usb_parser(void);
void usb_event_connect_callback(void *arg);
void usb_event_receive_callback(void *arg);
#endif 

