/**
 ****************************************************************************************************
 * @file        usbd_cdc_interface.c
 * @author      ??????????(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-06
 * @brief       USB CDC ????????
 * @license     Copyright (c) 2020-2032, ??????????????????????
 ****************************************************************************************************
 * @attention
 *
 * ?????:??????? MiniSTM32 V4??????
 * ???????:www.yuanzige.com
 * ???????:www.openedv.com
 * ??????:www.alientek.com
 * ??????:openedv.taobao.com
 *
 * ??????
 * V1.0 20200406
 * ?????????
 *
 ****************************************************************************************************
 */

#include "string.h"
#include "stdarg.h"
#include "stdio.h"
#include "usbd_cdc_interface.h"
#include "event_manager.h"
#include "drv_comm.h"
#include "app_cmd.h"
#include "cmd.h"
#include "usart.h"
#include "delay.h"
 
volatile bool usb_link = false;
/* USB????????????????? */
USBD_CDC_LineCodingTypeDef LineCoding =
{
    115200,     /* ?????? */
    0x00,       /* ????,???1?? */
    0x00,       /* ??????,????? */
    0x08        /* ??????,???8?? */
};


/* usb_printf?????????, ????vsprintf */
uint8_t g_usb_usart_printf_buffer[USB_USART_REC_LEN];
/* USB??????????????,???USART_REC_LEN?????,????USBD_CDC_SetRxBuffer???? */
uint8_t g_usb_rx_buffer[USB_USART_REC_LEN];

USB_MESSAGE_BOX usb_message;

volatile uint32_t usb_idle_tick;

/* ??????
 * bit15   , ?????????
 * bit14   , ?????0x0d
 * bit13~0 , ?????????????????
 */
uint16_t g_usb_usart_rx_sta=0;  /* ????????? */


extern USBD_HandleTypeDef USBD_Device;
static int8_t CDC_Itf_Init(void);
static int8_t CDC_Itf_DeInit(void);
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(uint8_t *pbuf, uint32_t *Len);


/* ??????????????(??USB??????) */
USBD_CDC_ItfTypeDef USBD_CDC_fops =
{
    CDC_Itf_Init,
    CDC_Itf_DeInit,
    CDC_Itf_Control,
    CDC_Itf_Receive
};

/**
 * @brief       ????? CDC
 * @param       ??
 * @retval      USB??
 *   @arg       USBD_OK(0)   , ????;
 *   @arg       USBD_BUSY(1) , ?;
 *   @arg       USBD_FAIL(2) , ???;
 */
static int8_t CDC_Itf_Init(void)
{
    USBD_CDC_SetRxBuffer(&USBD_Device, g_usb_rx_buffer);
    return USBD_OK;
}

/**
 * @brief       ???? CDC
 * @param       ??
 * @retval      USB??
 *   @arg       USBD_OK(0)   , ????;
 *   @arg       USBD_BUSY(1) , ?;
 *   @arg       USBD_FAIL(2) , ???;
 */
static int8_t CDC_Itf_DeInit(void)
{
    return USBD_OK;
}

/**
 * @brief       ???? CDC ??????
 * @param       cmd     : ????????
 * @param       buf     : ?????????????/????????H????
 * @param       length  : ???????
 * @retval      USB??
 *   @arg       USBD_OK(0)   , ????;
 *   @arg       USBD_BUSY(1) , ?;
 *   @arg       USBD_FAIL(2) , ???;
 */
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    switch (cmd)
    {
        case CDC_SEND_ENCAPSULATED_COMMAND:
            break;

        case CDC_GET_ENCAPSULATED_RESPONSE:
            break;

        case CDC_SET_COMM_FEATURE:
            break;

        case CDC_GET_COMM_FEATURE:
            break;

        case CDC_CLEAR_COMM_FEATURE:
            break;

        case CDC_SET_LINE_CODING:
            LineCoding.bitrate = (uint32_t) (pbuf[0] | (pbuf[1] << 8) |
                                             (pbuf[2] << 16) | (pbuf[3] << 24));
            LineCoding.format = pbuf[4];
            LineCoding.paritytype = pbuf[5];
            LineCoding.datatype = pbuf[6];
				    #if 0
            /* ??????????? */
            printf("linecoding.format:%d\r\n", LineCoding.format);
            printf("linecoding.paritytype:%d\r\n", LineCoding.paritytype);
            printf("linecoding.datatype:%d\r\n", LineCoding.datatype);
            printf("linecoding.bitrate:%d\r\n", LineCoding.bitrate);
				    #endif
            break;

        case CDC_GET_LINE_CODING:
            pbuf[0] = (uint8_t) (LineCoding.bitrate);
            pbuf[1] = (uint8_t) (LineCoding.bitrate >> 8);
            pbuf[2] = (uint8_t) (LineCoding.bitrate >> 16);
            pbuf[3] = (uint8_t) (LineCoding.bitrate >> 24);
            pbuf[4] = LineCoding.format;
            pbuf[5] = LineCoding.paritytype;
            pbuf[6] = LineCoding.datatype;
            break;

        case CDC_SET_CONTROL_LINE_STATE:
            break;

        case CDC_SEND_BREAK:
            break;

        default:
            break;
    }

    return USBD_OK;
}

/**
 * @brief       CDC ??????????
 * @param       buf     : ?????????????
 * @param       len     : ??????????????
 * @retval      USB??
 *   @arg       USBD_OK(0)   , ????;
 *   @arg       USBD_BUSY(1) , ?;
 *   @arg       USBD_FAIL(2) , ???;
 */
static int8_t CDC_Itf_Receive(uint8_t *buf, uint32_t *len)
{
    USBD_CDC_ReceivePacket(&USBD_Device);
    cdc_vcp_data_rx(buf, *len);
    return USBD_OK;
}

/**
 * @brief       ?????? USB ????????????????
 * @param       buf     : ?????????????
 * @param       len     : ??????????????
 * @retval      ??
 */
void cdc_vcp_data_rx (uint8_t *buf, uint32_t Len)
{
    if (buf == NULL || Len == 0)
    {
        return;
    }
    DrvUsbRing_Push(buf, Len);
    if (Cmd_IsYmodemActive() == 0U)
    {
        if (usb_message.g_user_usb_rx_len + Len > USB_USART_REC_LEN)
        {
            reset_usb_parser();
            return;
        }
        memcpy(usb_message.g_user_usb_rx_buffer + usb_message.g_user_usb_rx_len, buf, Len);
        usb_message.g_user_usb_rx_len += Len;
        usb_idle_tick = 5;
    }
}

/**
 * @brief       ??? USB ????????
 * @param       buf     : ???????????????
 * @param       len     : ???????
 * @retval      ??
 */
void cdc_vcp_data_tx(void *data, uint16_t Len)
{
	
    if(USBD_CDC_SetTxBuffer(&USBD_Device, (uint8_t*)data, Len) == USBD_FAIL)
				return;
		
    USBD_CDC_TransmitPacket(&USBD_Device);
}
extern volatile uint8_t g_device_state;     /* USB???? ??? */
/**
 * @brief       ??? USB ????????????
 *   @note      ???USB VCP???printf???
 *              ????????????????????USB_USART_REC_LEN???
 * @param       ????????
 * @retval      ??
 */
void usb_printf(char *fmt, ...)
{
    uint16_t i;
 
    va_list ap;
		va_start(ap, fmt);
		vsprintf((char *)g_usb_usart_printf_buffer, fmt, ap);
		va_end(ap);
		i = strlen((const char *)g_usb_usart_printf_buffer);    /* ??????????????? */
  	cdc_vcp_data_tx(g_usb_usart_printf_buffer, i);          /* ???????? */	
    //delay_ms(CDC_POLLING_INTERVAL);	
}
void reset_usb_parser(void)
{   
   usb_message.g_user_usb_rx_len = 0;
	 memset(usb_message.g_user_usb_rx_buffer,0,sizeof(usb_message.g_user_usb_rx_buffer));
	 frame_parser_init(&usb_message.usb_parser);
}

/* Task15: 帧处理器注册点 —— middleware 侧不直接调用 protocol 的 Cmd_ProcessFrame，
 * 由 protocol/cmd.c 经 Usb_RegisterFrameHandler 注册分发回调 */
static UsbFrameHandler s_usb_frame_handler = NULL;

void Usb_RegisterFrameHandler(UsbFrameHandler handler)
{
    s_usb_frame_handler = handler;
}

 
USBD_HandleTypeDef USBD_Device;             /* USB Device???????? */
 
void usb_event_connect_callback(void *arg)
{  
	 extern void beep_play_notice(void);
	 extern void beep_play_error(void); 
   static volatile uint8_t usbstatus;
	 if(usbstatus!=g_device_state)
	 { 
	    usbstatus = g_device_state;
		  if(usbstatus == 1)
      {
					/*usb ??????*/
				  usb_link = true;
				  beep_play_notice();
			}
			else
			{
				  /*usb ????*/
				  usb_link = false;
				  beep_play_error();
			}
	 }
}

void usb_cdc_init(void)
{ 
    usbd_port_config(0);    /* USB???? */
    delay_ms(1);
    usbd_port_config(1);    /* USB??????? */
    delay_ms(1);
    extern USBD_DescriptorsTypeDef VCP_Desc;
    USBD_Init(&USBD_Device, &VCP_Desc, 0);
    USBD_RegisterClass(&USBD_Device, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&USBD_Device, &USBD_CDC_fops);
    USBD_Start(&USBD_Device);
 
}

void usb_event_receive_callback(void *arg) {
	  USB_MESSAGE_BOX *message = (USB_MESSAGE_BOX*)arg;
	  if(message == NULL)return;
	  uint8_t *data = message->g_user_usb_rx_buffer;
	  uint16_t rx_len = message->g_user_usb_rx_len;
	  while (rx_len--){
      if(frame_parser_process_byte(&message->usb_parser, *data++)){
				/* 用 parser 已解析出的完整帧（buffer+index）做二次解析，
				 * 而非整批 buffer：一次 USB 接收可能含前导字节/多帧合并，
				 * 用整批长度解析会因长度/CRC 不匹配把指令帧全部丢弃 */
				_AGREEMENT frame;
				if(dataAgreeAnalys(&frame,message->usb_parser.buffer,message->usb_parser.index) == AGREE_MEN_OK)
			 {
					if (s_usb_frame_handler != NULL)
					{
						s_usb_frame_handler(&frame, cdc_vcp_data_tx);
					}
			 }
				/* 复位状态机以继续解析后续字节（保留已 malloc 的 buffer，
				 * 避免每次 frame_parser_init 重新 malloc/free）。
				 * 注意：不重置 calc_checksum/expected_length——下一帧 STATE_IDLE
				 * 遇 0x5A 会重建 checksum，STATE_DEST_ID 会覆盖 expected_length；
				 * 若未来调整状态机，此依赖须同步评估 */
				message->usb_parser.state = STATE_IDLE;
				message->usb_parser.index = 0U;
				message->usb_parser.data_bytes_received = 0U;
				message->usb_parser.frame_valid = false;
     }
	 }
		reset_usb_parser();   /* 内含 frame_parser_init：清空接收缓冲并重置解析器 */
    set_event_disable("usb_receive");
}
 
 







