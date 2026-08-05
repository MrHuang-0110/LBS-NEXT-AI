#include "pika_config.h"
#include "malloc.h"
#include "sys.h"
#include "stdarg.h"
extern void usb_printf(char *fmt, ...);
void pika_platform_free(void* ptr) {
    myfree(SRAMIN,ptr);
}
void* pika_platform_realloc(void* ptr, size_t size) {
 
    return myrealloc(SRAMIN,ptr, size);
}
void* pika_platform_malloc(size_t size) {
 
    return mymalloc(SRAMIN,size);
}
void pika_platform_printf(char* fmt, ...)
{ 
	  #include "usbd_cdc_interface.h"
	  
    uint16_t i;
 
    va_list ap;
		va_start(ap, fmt);
		vsprintf((char *)g_usb_usart_printf_buffer, fmt, ap);
		va_end(ap);
		i = strlen((const char *)g_usb_usart_printf_buffer);    /* 此次发送数据的长度 */
  	cdc_vcp_data_tx(g_usb_usart_printf_buffer, i);          /* 发送数据 */	
}
	