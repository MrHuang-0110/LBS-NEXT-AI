
#ifndef __KEY_H
#define __KEY_H

#include "./SYSTEM/sys/sys.h"
#include <stdbool.h>
// ����״̬�����ṹ��
typedef struct {
    // ����Ӳ������
    GPIO_TypeDef* port;
    uint16_t pin;
    GPIO_PinState active_level;  // ����ʱ�ĵ�ƽ
    
    // ״̬����
    enum {
        KEY_STATE_IDLE = 0,      // ����
        KEY_STATE_PRESS_DB,      // ��������
        KEY_STATE_PRESSED,       // �Ѱ���
        KEY_STATE_LONG_PRESS,    // ��������
        KEY_STATE_RELEASE_DB     // �ͷ�����
    } state;
    
    // �¼���־
    volatile uint8_t short_press_flag;
    volatile uint8_t long_press_flag;
    volatile uint8_t long_hold_flag;
    
    // ʱ���¼
    uint32_t press_start_time;
    uint32_t press_duration;
    uint32_t last_long_hold_time;
    
    // ���ò���
    uint16_t debounce_time;      // ����ʱ��(ms)
    uint16_t long_press_time;    // �����ж�ʱ��(ms)
    uint16_t long_hold_interval; // �������ַ������(ms)
    
    // ����ԭʼ״̬
    uint8_t current_raw_state;
    uint8_t last_raw_state;
} Key_Scan_Handle_t;

typedef struct{ 
  int advance_offset1,advance_offset2;
	int retreat_offset1,retreat_offset2;
}REMOTE_CFG;

/******************************************************************************************/
/* ���� ���� */
#if WIALL_HARDWARE_ENABLE
	#define KEY1_GPIO_PORT                  GPIOC
	#define KEY1_GPIO_PIN                   GPIO_PIN_4
	#define KEY1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC��ʱ��ʹ�� */

	#define KEY2_GPIO_PORT                  GPIOC
	#define KEY2_GPIO_PIN                   GPIO_PIN_3
	#define KEY2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

	#define KEY3_GPIO_PORT                  GPIOC
	#define KEY3_GPIO_PIN                   GPIO_PIN_2
	#define KEY3_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */
	
	#define BLUE_STA_PORT                  GPIOA
	#define BLUE_STA_GPIO_PIN              GPIO_PIN_8
	#define BLUE_STA_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */	
#else
	#define KEY0_GPIO_PORT                  GPIOC
	#define KEY0_GPIO_PIN                   GPIO_PIN_5
	#define KEY0_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)   /* PC��ʱ��ʹ�� */

	#define KEY1_GPIO_PORT                  GPIOA
	#define KEY1_GPIO_PIN                   GPIO_PIN_15
	#define KEY1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */

	#define WKUP_GPIO_PORT                  GPIOA
	#define WKUP_GPIO_PIN                   GPIO_PIN_0
	#define WKUP_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA��ʱ��ʹ�� */
#endif

 

/******************************************************************************************/

#define KEY0        HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN)     /* ��ȡKEY0���� 0���� 1���� �м�*/
#define KEY1        HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN)     /* ��ȡKEY1���� 0���£�1���� ���*/
#define WK_UP       HAL_GPIO_ReadPin(KEY3_GPIO_PORT, KEY3_GPIO_PIN)     /* ��ȡWKUP���� 0���£�1���� �Ҽ�*/
#define BLUE_STA    HAL_GPIO_ReadPin(BLUE_STA_PORT, BLUE_STA_GPIO_PIN)     /* ��ȡWKUP���� 0���£�1���� �Ҽ�*/
 
void key_init(void);                /* ������ʼ������ */
void Key_Config_Params(uint16_t debounce_ms, uint16_t long_press_ms, uint16_t hold_interval_ms);
void Key_Reset_State(void);
uint8_t Key_Is_Pressed(void);
uint32_t Key_Get_Press_Duration(void);
uint8_t Key_Check_Long_Hold(void);
uint8_t Key_Check_Long_Press(void);
uint8_t Key_Check_Short_Press(void);
void Key_Scan_Handler(uint32_t scan_interval_ms);
void key_middle_callback(void *arg);
void set_entery_short(uint8_t key);

extern volatile bool start_py;
extern volatile bool start_pauto;
 
void loader_remote_cfg(void);
void write_advance_remote_cfg(int offset1,int offset2);
void write_retreat_remote_cfg(int offset1,int offset2);
int read_advance_offset1(void);
int read_advance_offset2(void);
int read_retreat_offset1(void);
int read_retreat_offset2(void);
#endif


















