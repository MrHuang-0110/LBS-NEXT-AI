#include "btim.h"
#include "event_manager.h"
#include "stdlib.h"
#include <string.h>
volatile uint32_t cpuTick;
volatile uint32_t userCPUTick;
TIM_HandleTypeDef g_timx_handle;  /* ???????? */
 
 
// ???????
static volatile DateTime_t sys_time;  // ?????volatile???��????
static volatile uint32_t tick_counter = 0; // ??????????????????
static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// ?��?????
static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// ???????????
static uint8_t get_month_days(uint16_t year, uint8_t month) {
    if (month == 2 && is_leap_year(year))
        return 29;
    else
        return month_days[month - 1];
}

// ???????1??????????????��??��????
static void time_add_one_second(void) {
    sys_time.second++;
    if (sys_time.second < 60) return;
    sys_time.second = 0;
    sys_time.minute++;
    if (sys_time.minute < 60) return;
    sys_time.minute = 0;
    sys_time.hour++;
    if (sys_time.hour < 24) return;
    sys_time.hour = 0;
    sys_time.day++;
    uint8_t max_day = get_month_days(sys_time.year, sys_time.month);
    if (sys_time.day <= max_day) return;
    sys_time.day = 1;
    sys_time.month++;
    if (sys_time.month <= 12) return;
    sys_time.month = 1;
    sys_time.year++;
}
 void Time_LoadFromFlash(void)
{ 
		   sys_time.year = 2026;
			 sys_time.month = 3;
			 sys_time.day = 30;
			 sys_time.hour = 15;
			 sys_time.minute = 0;
			 sys_time.second = 0;
}
void Time_Init(void) {    
    Time_LoadFromFlash(); 
}

// ????????????????��??????
void Time_Set(DateTime_t *dt) {
    __disable_irq();
    memcpy((void*)&sys_time, dt, sizeof(DateTime_t));
    __enable_irq();
}

// ???????????��?????
void Time_Get(DateTime_t *dt) {
    __disable_irq();
    memcpy(dt, (void*)&sys_time, sizeof(DateTime_t));
    __enable_irq();
}
/**
 * @brief       ?????????TIMX????��?????????
 * @note
 *              ??????????????????APB1,??PPRE1 ?? 2????????
 *              ???????????????APB1????2??, ??APB1?36M, ??????????? = 72Mhz
 *              ????????????????: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=????????????,??��:Mhz
 *
 * @param       arr: ?????????
 * @param       psc: ?????????
 * @retval      ??
 */
void btim_timx_int_init(uint16_t arr, uint16_t psc)
{
    (void)HAL_TIM_Base_Stop_IT(&g_timx_handle);
    (void)HAL_TIM_Base_DeInit(&g_timx_handle);
    memset(&g_timx_handle, 0, sizeof(g_timx_handle));

    g_timx_handle.Instance = BTIM_TIMX_INT;
    g_timx_handle.Init.Prescaler = psc;
    g_timx_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_timx_handle.Init.Period = arr;
    g_timx_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_timx_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    (void)HAL_TIM_Base_Init(&g_timx_handle);
}

void btim_timx_int_start(void)
{
    (void)HAL_TIM_Base_Start_IT(&g_timx_handle);
}

/**
 * @brief       ???????????????????????????��??????
                ???????HAL_TIM_Base_Init()????????
 * @param       htim:????????
 * @retval      ??
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIMX_INT)
    {
        BTIM_TIMX_INT_CLK_ENABLE();                     /* ???TIM??? */
        HAL_NVIC_SetPriority(BTIM_TIMX_INT_IRQn, 2, 0); /* ???1?????????3????2 */
        HAL_NVIC_EnableIRQ(BTIM_TIMX_INT_IRQn);         /* ????ITM3?��? */
    }
}

/**
 * @brief       ?????TIMX?��??????
 * @param       ??
 * @retval      ??
 */
void BTIM_TIMX_INT_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&g_timx_handle); /* ??????��???????????? */
}

/**
 * @brief       ??????????��???????
 * @param       htim:????????
 * @retval      ??
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIMX_INT)
    {
			  cpuTick++;	
			  userCPUTick++;
        extern void event_schedlucer(uint32_t cpu_tick);
			  event_schedlucer(cpuTick);
			  extern volatile uint32_t usb_idle_tick;
			  if(usb_idle_tick > 0)
				{
				   usb_idle_tick--;
					 if(usb_idle_tick == 0)
					 {
							set_event_enable("usb_receive");
					 }
				}
				if(cpuTick%1000 == 0)
				{ 
					time_add_one_second();
				tick_counter++;			  
				}
 
    }
}

uint32_t getTim6Tick(void)
{ 
   return cpuTick;
}

void resetUserCPUTick(void)
{ 
   userCPUTick = 0;
}
uint32_t getUserCPUTick(void)
{ 
  return userCPUTick;
}
/*reg config*/

static void RCC_Configuration(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN |RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN | RCC_APB1ENR_TIM2EN |RCC_APB1ENR_TIM5EN;
	
 
	  /*?????TIM2*/
	  sys_gpio_remap_set(8,2,3);
	  /*?????TIM3*/
	  sys_gpio_remap_set(10,2,3);
}

static void gtim_timx_pwm_chy_init(uint16_t arr, uint16_t psc,uint8_t chy,TIM_TypeDef *GTIM_TIMX_PWM)
{
 
    GTIM_TIMX_PWM->ARR = arr;       /* ?څ????????????? */
    GTIM_TIMX_PWM->PSC = psc;       /* ??????????  */
    GTIM_TIMX_PWM->BDTR |= 1 << 15; /* ???MOE��(??TIM1/8 ?��?????,????????MOE???????PWM), ???????????, ???
                                     * ?????????��??, ????????/?????��????????, ???????????????????MOE��
                                     */

    if (chy <= 2)
    {
        GTIM_TIMX_PWM->CCMR1 |= 6 << (4 + 8 * (chy - 1));   /* CH1/2 PWM??1 */
        GTIM_TIMX_PWM->CCMR1 |= 1 << (3 + 8 * (chy - 1));   /* CH1/2 ??????? */
    }
    else if (chy <= 4)
    {
        GTIM_TIMX_PWM->CCMR2 |= 6 << (4 + 8 * (chy - 3));   /* CH3/4 PWM??1 */
        GTIM_TIMX_PWM->CCMR2 |= 1 << (3 + 8 * (chy - 3));   /* CH3/4 ??????? */
    }

    GTIM_TIMX_PWM->CCER |= 1 << (4 * (chy - 1));        /* OCy ?????? */
		
  //  GTIM_TIMX_PWM->CCER |= 1 << (1 + 4 * (chy - 1));    /* OCy ??????�� */
		GTIM_TIMX_PWM->CCER &=  ~(1 << (1 + 4 * (chy - 1))); /*??????��*/
		
    GTIM_TIMX_PWM->CR1 |= 1 << 7;   /* ARPE??? */
    GTIM_TIMX_PWM->CR1 |= 1 << 0;   /* ???????TIMX */
}

static void GPIO_Configuration(void)
{
      
	     
	
	    sys_gpio_set(GPIOA,
									 SYS_GPIO_PIN15,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
 
		  sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN3,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
	
	
 
	    sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN6,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
 
		  sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN7,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
									 
 
	    sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN8,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);

		  sys_gpio_set(GPIOC,
									 SYS_GPIO_PIN9,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
									 
									 
	    sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN8,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									 
	    sys_gpio_set(GPIOB,
									 SYS_GPIO_PIN9,
									 SYS_GPIO_MODE_AF,
									 SYS_GPIO_OTYPE_PP,
									 SYS_GPIO_SPEED_HIGH,
									 SYS_GPIO_PUPD_PU);
									
}
 
void pwm_init(void)
{ 
   RCC_Configuration();
	 GPIO_Configuration();
 
	 gtim_timx_pwm_chy_init(100-1,720-1,1,TIM2);
	 gtim_timx_pwm_chy_init(100-1,720-1,2,TIM2);
	 gtim_timx_pwm_chy_init(100-1,720-1,3,TIM4);
	 gtim_timx_pwm_chy_init(100-1,720-1,4,TIM4);	
	
	 gtim_timx_pwm_chy_init(100-1,720-1,1,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,2,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,3,TIM3);
	 gtim_timx_pwm_chy_init(100-1,720-1,4,TIM3);
}


 

void pwm_set_output(uint8_t id, int pwm)
{ 
    uint16_t pwm1 = 0, pwm2 = 0;
    
    if(pwm > PWM_MAX) pwm = PWM_MAX;
    if(pwm < -PWM_MAX) pwm = -PWM_MAX;
    
    // ??????��??????
    if(pwm > 0) {
        // ???????1???PWM?????2???????
        pwm1 = pwm;    // PWM????
        pwm2 = 0;      // 0%???? = ????????
    } else if(pwm < 0) {
        // ???????1????????????2???PWM
        pwm1 = 0;      // 0%???? = ????????
        pwm2 = -pwm;   // PWM????
    } else {
        // pwm=0??????????????????��?????????
        pwm1 = 0;
        pwm2 = 0;
    }
    
    // ????ID???????????
    switch(id) {
        case 4:
            //TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM2->CCR1 = pwm1;
            TIM2->CCR2 = pwm2;
            break;
        case 5:
            //TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = pwm1;
            TIM3->CCR2 = pwm2;
            break;
        case 6:
            //TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = pwm1;
            TIM3->CCR4 = pwm2;
            break;
        case 7:
            //TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = pwm1;
            TIM4->CCR4 = pwm2;
            break;
    }
}

 
void pwm_stop_slide(uint8_t id)
{
    switch(id) {
        case 4:
           // TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
						TIM2->CCR1 = 0;
            TIM2->CCR2 = 0;				
            break;
        case 5:
						//TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = 0;
            TIM3->CCR2 = 0;
            break;
        case 6:
						//TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = 0;
            TIM3->CCR4 = 0;
            break;
        case 7:
						//TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = 0;
            TIM4->CCR4 = 0;
            break;
    }
}
 
void pwm_stop_breaking(uint8_t id)
{ 
    switch(id) {
        case 4:
           // TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM2->CCR1 = PWM_MAX;  // 100%???? = ????????
            TIM2->CCR2 = PWM_MAX;  // 100%???? = ????????
				    
            break;
        case 5:
            //TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
            TIM3->CCR1 = PWM_MAX;
            TIM3->CCR2 = PWM_MAX;
            break;
        case 6:
            //TIM3->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM3->CCR3 = PWM_MAX;
            TIM3->CCR4 = PWM_MAX;
            break;
        case 7:
            //TIM4->CCER |= TIM_CCER_CC3E | TIM_CCER_CC4E;
            TIM4->CCR3 = PWM_MAX;
            TIM4->CCR4 = PWM_MAX;
            break;
    }  
}
	
