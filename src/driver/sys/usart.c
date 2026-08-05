 
#include "sys.h"
#include "usart.h"
#include "delay.h"
#include "frame.h"
#include "device_pool.h"
#include "drv_comm.h"
#include "stdarg.h"
extern void usb_printf(char *fmt, ...);
static UART_HandleTypeDef g_uart1_handle;  /* UART??? */
static UART_HandleTypeDef g_uart2_handle;  /* UART??? */
static UART_HandleTypeDef g_uart3_handle;  /* UART??? */
static UART_HandleTypeDef g_uart4_handle;  /* UART??? */
static UART_HandleTypeDef g_uart5_handle;  /* UART??? */


static DMA_HandleTypeDef  UART1RxDMA_Handler;
static DMA_HandleTypeDef  UART2RxDMA_Handler;
static DMA_HandleTypeDef  UART3RxDMA_Handler;
static DMA_HandleTypeDef  UART4RxDMA_Handler;

static uint8_t usart1dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart2dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart3dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart4dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint8_t usart5dmaRxBufer[DMA_RX_BUFER_SIZE];
static uint32_t usart5dmaRxlength;

static uint8_t usart5RxBufer[RXBUFFERSIZE];

static uint8_t blueTxBufer[512];

UART_HandleTypeDef *getusartHandle(uint8_t num)
{ 
   switch(num)
	 {
	   case 1:return &g_uart1_handle;
		 case 2:return &g_uart2_handle;
		 case 3:return &g_uart3_handle;
		 case 4:return &g_uart4_handle;
		 case 5:return &g_uart5_handle;
		 default:break;
	 }
	 return NULL;
}
/******************************************************************************************/
/* ???????????, ???printf????, ??????????use MicroLIB */

#if 1

#if (__ARMCC_VERSION >= 6010050)            /* ???AC6??????? */
__asm(".global __use_no_semihosting\n\t");  /* ???????????????? */
__asm(".global __ARM_use_no_argv \n\t");    /* AC6?????????main????????????????????????????????????? */

#else
/* ???AC5???????, ?????????__FILE ?? ???????????? */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* ??????????????????????????_ttywrch\_sys_exit\_sys_command_string????,????????AC6??AC5?? */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/* ????_sys_exit()??????????????? */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}


/* FILE ?? stdio.h???��??. */
FILE __stdout;

/* MDK??????????fputc????, printf????????????????fputc?????????????? */
int fputc(int ch, FILE *f)
{
#if 0
	uint8_t bufer[1];
	bufer[0] = (uint8_t)ch;

 
    extern void cdc_vcp_data_tx(void *data, uint16_t Len);
  	cdc_vcp_data_tx(bufer, 1);          /* ???????? */	
	#endif
    return ch;
}
void blue_printf(const char *format, ...)
{ 
	va_list args;					// va_list?????,???????????
  uint32_t length;				// ????????


  va_start(args, format);
	length = vsnprintf((char *)blueTxBufer, 512, (char *)format, args);
  va_end(args);
 	uart_transmit_it(&g_uart5_handle,blueTxBufer,length);
	 //HAL_UART_Transmit(uartDevices[BLUE].huart, UserTxBufferHS, length,1000);		
}
#endif


uint8_t uart_transmit_it(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    if ((huart == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }
    if (HAL_UART_GetState(huart) != HAL_UART_STATE_READY)
    {
        return 0U;
    }
    if (HAL_UART_Transmit_IT(huart, data, len) != HAL_OK)
    {
        return 0U;
    }
    return 1U;
}
/******************************************************************************************/

 
 
/**
 * @brief       ????X?????????
 * @param       baudrate: ??????, ?????????????��??????
 * @note        ???: ?????????????????, ??????????????????.
 *              ?????USART????????sys_stm32_clock_init()????????????��???.
 * @retval      ??
 */
void usart1_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart1_handle.Instance = USART1_UX;                                       /* USART_UX */
    g_uart1_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8��?????? */
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;                           /* ?????�� */
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;                            /* ?????��??�� */
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart1_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart2_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart2_handle.Instance = USART2_UX;                                       /* USART_UX */
    g_uart2_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8��?????? */
    g_uart2_handle.Init.StopBits = UART_STOPBITS_1;                           /* ?????�� */
    g_uart2_handle.Init.Parity = UART_PARITY_NONE;                            /* ?????��??�� */
    g_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart2_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart2_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart3_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart3_handle.Instance = USART3_UX;                                       /* USART_UX */
    g_uart3_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart3_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8��?????? */
    g_uart3_handle.Init.StopBits = UART_STOPBITS_1;                           /* ?????�� */
    g_uart3_handle.Init.Parity = UART_PARITY_NONE;                            /* ?????��??�� */
    g_uart3_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart3_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart3_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart4_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart4_handle.Instance = USART4_UX;                                       /* USART_UX */
    g_uart4_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart4_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8��?????? */
    g_uart4_handle.Init.StopBits = UART_STOPBITS_1;                           /* ?????�� */
    g_uart4_handle.Init.Parity = UART_PARITY_NONE;                            /* ?????��??�� */
    g_uart4_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart4_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart4_handle);                                           /* HAL_UART_Init()?????UART1 */
}

void usart5_init(uint32_t baudrate)
{
    /*UART ?????????*/
    g_uart5_handle.Instance = USART5_UX;                                       /* USART_UX */
    g_uart5_handle.Init.BaudRate = baudrate;                                  /* ?????? */
    g_uart5_handle.Init.WordLength = UART_WORDLENGTH_8B;                      /* ????8��?????? */
    g_uart5_handle.Init.StopBits = UART_STOPBITS_1;                           /* ?????�� */
    g_uart5_handle.Init.Parity = UART_PARITY_NONE;                            /* ?????��??�� */
    g_uart5_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;                      /* ????????? */
    g_uart5_handle.Init.Mode = UART_MODE_TX_RX;                               /* ????? */
    HAL_UART_Init(&g_uart5_handle);                                           /* HAL_UART_Init()?????UART1 */   
}
void uart_blue_idle_start(void)
{ 	
	__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_IDLE); 
	__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_RXNE);  
}
void uart_dma_idle_start(void)
{ 
	__HAL_UART_ENABLE_IT(&g_uart1_handle,UART_IT_IDLE);/*?????????��?*/	 
  HAL_UART_Receive_DMA(&g_uart1_handle,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart2_handle,UART_IT_IDLE);/*?????????��?*/	 
  HAL_UART_Receive_DMA(&g_uart2_handle,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart3_handle,UART_IT_IDLE);/*?????????��?*/	 
  HAL_UART_Receive_DMA(&g_uart3_handle,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	__HAL_UART_ENABLE_IT(&g_uart4_handle,UART_IT_IDLE);/*?????????��?*/	 
  HAL_UART_Receive_DMA(&g_uart4_handle,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);	
	
	//__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_IDLE); 
	//__HAL_UART_ENABLE_IT(&g_uart5_handle,UART_IT_RXNE); 
	
}
/**
 * @brief       UART???????????
 * @param       huart: UART??????????
 * @note        ???????HAL_UART_Init()????
 *              ????????????????????��?????
 * @retval      ??
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
     GPIO_InitTypeDef gpio_init_struct;
    __HAL_RCC_AFIO_CLK_ENABLE();
	   		 
	  
    if (huart->Instance == USART1_UX)                        
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART1_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART1_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART1_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART1_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART1_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART1_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART1_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????��??????? */
        
        __HAL_LINKDMA(&g_uart1_handle,hdmarx,UART1RxDMA_Handler);  
        UART1RxDMA_Handler.Instance=DMA1_Channel5;                           //??????
        UART1RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //?��????????
        UART1RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART1RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //?��????????
        UART1RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8��
        UART1RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //?��?????????:8��
        UART1RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART1RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?��??????
        HAL_DMA_DeInit(&UART1RxDMA_Handler);
        HAL_DMA_Init(&UART1RxDMA_Handler);
			     		 
		    HAL_NVIC_EnableIRQ(USART1_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART1_UX_IRQn,1,0);
    }
		
		
    if (huart->Instance == USART2_UX)                           
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART2_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART2_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART2_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART2_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART2_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART2_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART2_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????��??????? */
        
				__HAL_LINKDMA(&g_uart2_handle,hdmarx,UART2RxDMA_Handler);  
        UART2RxDMA_Handler.Instance=DMA1_Channel6;                           //??????
        UART2RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //?��????????
        UART2RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART2RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //?��????????
        UART2RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8��
        UART2RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //?��?????????:8��
        UART2RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART2RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?��??????
        HAL_DMA_DeInit(&UART2RxDMA_Handler);
        HAL_DMA_Init(&UART2RxDMA_Handler);
			     		 
		    HAL_NVIC_EnableIRQ(USART2_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART2_UX_IRQn,1,0);
        HAL_UART_Receive_DMA(&g_uart2_handle,usart2dmaRxBufer,DMA_RX_BUFER_SIZE); 
    }
    if (huart->Instance == USART3_UX)                           
    {
			  __HAL_RCC_DMA1_CLK_ENABLE();	
        USART3_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART3_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART3_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART3_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART3_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART3_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART3_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????��??????? */
				
				__HAL_LINKDMA(&g_uart3_handle,hdmarx,UART3RxDMA_Handler);
        UART3RxDMA_Handler.Instance=DMA1_Channel3;                           //??????
        UART3RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //?��????????
        UART3RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART3RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //?��????????
        UART3RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8��
        UART3RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //?��?????????:8��
        UART3RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART3RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?��??????
        HAL_DMA_DeInit(&UART3RxDMA_Handler);
        HAL_DMA_Init(&UART3RxDMA_Handler);
			       		 
		    HAL_NVIC_EnableIRQ(USART3_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART3_UX_IRQn,1,0);
        HAL_UART_Receive_DMA(&g_uart3_handle,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);		 
    }
    if (huart->Instance == USART4_UX)                           
    {
			  __HAL_RCC_DMA2_CLK_ENABLE();	
        USART4_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART4_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART4_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART4_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART4_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART4_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART4_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????��??????? */

				__HAL_LINKDMA(&g_uart4_handle,hdmarx,UART4RxDMA_Handler);
        UART4RxDMA_Handler.Instance=DMA2_Channel3;                           //??????
        UART4RxDMA_Handler.Init.Direction=DMA_PERIPH_TO_MEMORY;              //?��????????
        UART4RxDMA_Handler.Init.PeriphInc=DMA_PINC_DISABLE;                  //???????????
        UART4RxDMA_Handler.Init.MemInc=DMA_MINC_ENABLE;                      //?��????????
        UART4RxDMA_Handler.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;     //???????????:8��
        UART4RxDMA_Handler.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;        //?��?????????:8��
        UART4RxDMA_Handler.Init.Mode=DMA_CIRCULAR;                           //?????????
        UART4RxDMA_Handler.Init.Priority=DMA_PRIORITY_MEDIUM;                //?��??????
        HAL_DMA_DeInit(&UART4RxDMA_Handler);
        HAL_DMA_Init(&UART4RxDMA_Handler);
			       		 
		    HAL_NVIC_EnableIRQ(USART4_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART4_UX_IRQn,1,0);	 
    }
    if (huart->Instance == USART5_UX)                           
    {
        USART5_TX_GPIO_CLK_ENABLE();                             /* ??????TX????? */
        USART5_RX_GPIO_CLK_ENABLE();                             /* ??????RX????? */
        USART5_UX_CLK_ENABLE();                                  /* ????????? */
		
        gpio_init_struct.Pin = USART5_TX_GPIO_PIN;               /* ???????????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* ??????????? */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* ???? */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* IO???????????? */
        HAL_GPIO_Init(USART5_TX_GPIO_PORT, &gpio_init_struct);
                
        gpio_init_struct.Pin = USART5_RX_GPIO_PIN;               /* ????RX?? ?????? */
        gpio_init_struct.Mode = GPIO_MODE_AF_INPUT;    
        HAL_GPIO_Init(USART5_RX_GPIO_PORT, &gpio_init_struct);   /* ????RX?? ???????��??????? */
         
		    HAL_NVIC_EnableIRQ(USART5_UX_IRQn);				                      
		    HAL_NVIC_SetPriority(USART5_UX_IRQn,0,4);
    }
}
 
static void HAL_USART_IDLE_INTERRUPT(UART_HandleTypeDef *huart)
{
	  if ((huart->Instance == USART5_UX) &&
	      (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET))
		{
			  __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_RXNE);
			  if(usart5dmaRxlength >= DMA_RX_BUFER_SIZE)
				{ 
				   usart5dmaRxlength = 0;
					 memset(usart5dmaRxBufer,0,sizeof(usart5dmaRxBufer));			 
				}
				else
				{ 
				   usart5dmaRxBufer[usart5dmaRxlength++] = (uint8_t)(huart->Instance->DR & 0xFF);
				}	 
		}
	  if(__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE) != RESET)
		{ 
			 _AGREEMENT frame;
			 if(huart->Instance == USART1_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);					 
				   if(dataAgreeAnalys(&frame,usart1dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
						  SensorBase *base = HubBase_And_identify(0,frame.sID);
						  if(base!=NULL)
							{ 
							  set_sensor_parameter(base,(_AGREEMENT *)&frame);
							}
					 }
					 else
					 { 
					    /*????��??????*/;
					 // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart1dmaRxBufer,0,sizeof(usart1dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART2_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart2dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
						  SensorBase *base = HubBase_And_identify(1,frame.sID);
						  if(base!=NULL)
							{ 
							  set_sensor_parameter(base,(_AGREEMENT *)&frame);
							}
					 }
					 else
					 { 
					   // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart2dmaRxBufer,0,sizeof(usart2dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART3_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart3dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
						  SensorBase *base = HubBase_And_identify(2,frame.sID);
						  if(base!=NULL)
							{ 
							  set_sensor_parameter(base,(_AGREEMENT *)&frame);
							}
					 }
					 else
					 { 
					  // usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart3dmaRxBufer,0,sizeof(usart3dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART4_UX)
			 {
					__HAL_UART_CLEAR_IDLEFLAG(huart);
					HAL_UART_DMAStop(huart);	
				   if(dataAgreeAnalys(&frame,usart4dmaRxBufer,(DMA_RX_BUFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx))))
					 { 
						  SensorBase *base = HubBase_And_identify(3,frame.sID);
						  if(base!=NULL)
							{ 
							  set_sensor_parameter(base,(_AGREEMENT *)&frame);
							}
					 }
					 else
					 { 
					  //  usb_printf("%s",usart1dmaRxBufer);
					 }
					 memset(usart4dmaRxBufer,0,sizeof(usart4dmaRxBufer));
					 HAL_UART_Receive_DMA(huart,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);
			 }
			 if(huart->Instance == USART5_UX)
			 {
				   __HAL_UART_CLEAR_IDLEFLAG(huart);
				   if(dataAgreeAnalys(&frame,usart5dmaRxBufer,usart5dmaRxlength))
					 { 
						 set_sensor_parameter(getHubBase(8),(_AGREEMENT*)&frame);
					 }
					 else
					 { 
					   #include "blue.h"
						 DEV_BLUE *blue = (DEV_BLUE*)getHubBase(8);
						 DrvBtRing_Push(usart5dmaRxBufer, usart5dmaRxlength);
						 if (blue != NULL)
						 {
						   memset(blue->at_cmd_bufer,0,sizeof(blue->at_cmd_bufer));
						   if(usart5dmaRxlength < sizeof(blue->at_cmd_bufer))
						   {
						     memcpy(blue->at_cmd_bufer,usart5dmaRxBufer,usart5dmaRxlength);
						   }
						   else
						   {
						     memcpy(blue->at_cmd_bufer,usart5dmaRxBufer,sizeof(blue->at_cmd_bufer) - 1U);
						   }
						   blue->is_resh_flag = true;
						 }
					 }				 
					 usart5dmaRxlength = 0;
					 memset(usart5dmaRxBufer,0,DMA_RX_BUFER_SIZE);
					 volatile uint32_t tmp = huart->Instance->SR;
					 tmp = huart->Instance->DR;
					(void)tmp;				
			 }
		}
		
}
static void ClearUARTErrors(UART_HandleTypeDef *huart)
{
 
	
   // ?????????
   //__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF |UART_CLEAR_IDLEF);

   __HAL_UART_CLEAR_OREFLAG(huart);
	 __HAL_UART_CLEAR_IDLEFLAG(huart);
   huart->RxState = HAL_UART_STATE_READY;
   huart->Lock = HAL_UNLOCKED;
}

 void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	__HAL_UART_CLEAR_FEFLAG(huart);
	__HAL_UART_CLEAR_NEFLAG(huart);
	__HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_IDLEFLAG(huart);
	
	huart->RxState = HAL_UART_STATE_READY;
	huart->Lock = HAL_UNLOCKED;

  if(huart->Instance == USART1_UX)
  {     
		ClearUARTErrors(&g_uart1_handle);
    HAL_UART_Receive_DMA(huart,usart1dmaRxBufer,DMA_RX_BUFER_SIZE);
  }
  else if(huart->Instance == USART2_UX)
  {
		ClearUARTErrors(&g_uart2_handle);
     HAL_UART_Receive_DMA(huart,usart2dmaRxBufer,DMA_RX_BUFER_SIZE);
 }
	else if(huart->Instance == USART3_UX)
	{
		 ClearUARTErrors(&g_uart3_handle);
		 HAL_UART_Receive_DMA(huart,usart3dmaRxBufer,DMA_RX_BUFER_SIZE);
	}
	 else if(huart->Instance == USART4_UX)
	{
		 ClearUARTErrors(&g_uart4_handle);
		 HAL_UART_Receive_DMA(huart,usart4dmaRxBufer,DMA_RX_BUFER_SIZE);
	}
	else if(huart->Instance == USART5_UX)
	{
		 ClearUARTErrors(&g_uart5_handle);
		 huart->RxState = HAL_UART_STATE_READY;
		 huart->gState = HAL_UART_STATE_READY;
		 huart->Lock = HAL_UNLOCKED;
	}
}
void USART1_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart1_handle);
    HAL_UART_IRQHandler(&g_uart1_handle);   
}
void USART2_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart2_handle);
    HAL_UART_IRQHandler(&g_uart2_handle); 
}
void USART3_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart3_handle);
    HAL_UART_IRQHandler(&g_uart3_handle);   
}
void USART4_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart4_handle);
    HAL_UART_IRQHandler(&g_uart4_handle);  
}
void USART5_UX_IRQHandler(void)
{
	  HAL_USART_IDLE_INTERRUPT(&g_uart5_handle);
    if (g_uart5_handle.gState == HAL_UART_STATE_BUSY_TX)
    {
        HAL_UART_IRQHandler(&g_uart5_handle);
    }
}
void uart_port_init(void)
{ 
 	 usart1_init(115200);
	 usart2_init(115200);
	 usart3_init(115200);
	 usart4_init(115200);
	 usart5_init(115200);  
	 uart_blue_idle_start();
	 //uart_dma_idle_start();
}