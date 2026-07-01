/**
 ****************************************************************************************************
 * @file        sys.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-17
 * @brief       系统初始化代码(包括时钟配置/中断管理/GPIO设置等)
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 MiniSTM32 V4开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211103
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"


/**
 * @brief       设置中断向量表偏移地址
 * @param       baseaddr: 基址
 * @param       offset: 偏移量(必须是0, 或者0X100的倍数)
 * @retval      无
 */
void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset)
{
    /* 设置NVIC的向量表偏移寄存器,VTOR低9位保留,即[8:0]保留 */
    SCB->VTOR = baseaddr | (offset & (uint32_t)0xFFFFFE00);
}

/**
 * @brief       执行: WFI指令(执行完该指令进入低功耗状态, 等待中断唤醒)
 * @param       无
 * @retval      无
 */
void sys_wfi_set(void)
{
    __ASM volatile("wfi");
}

/**
 * @brief       关闭所有中断(但是不包括fault和NMI中断)
 * @param       无
 * @retval      无
 */
void sys_intx_disable(void)
{
    __ASM volatile("cpsid i");
}

/**
 * @brief       开启所有中断
 * @param       无
 * @retval      无
 */
void sys_intx_enable(void)
{
    __ASM volatile("cpsie i");
}

/**
 * @brief       设置栈顶地址
 * @note        左侧的红X, 属于MDK误报, 实际是没问题的
 * @param       addr: 栈顶地址
 * @retval      无
 */
void sys_msr_msp(uint32_t addr)
{
    __set_MSP(addr);  /* 设置栈顶地址 */
}

/**
 * @brief       进入待机模式
 * @param       无
 * @retval      无
 */
void sys_standby(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();    /* 使能电源时钟 */
    SET_BIT(PWR->CR, PWR_CR_PDDS); /* 进入待机模式 */
}

/**
 * @brief       系统软复位
 * @param       无
 * @retval      无
 */
void sys_soft_reset(void)
{
    NVIC_SystemReset();
}
/**
 * @brief       GPIO重映射功能选择设置
 *   @note      这里仅支持对MAPR寄存器的配置, 不支持对MAPR2寄存器的配置!!!
 * @param       pos: 在AFIO_MAPR寄存器里面的起始位置, 0~24
 *   @arg       [0]    , SPI1_REMAP;         [1]    , I2C1_REMAP;         [2]    , USART1_REMAP;        [3]    , USART2_REMAP;
 *   @arg       [5:4]  , USART3_REMAP;       [7:6]  , TIM1_REMAP;         [9:8]  , TIM2_REMAP;          [11:10], TIM3_REMAP;
 *   @arg       [12]   , TIM4_REMAP;         [14:13], CAN_REMAP;          [15]   , PD01_REMAP;          [16]   , TIM15CH4_REMAP;
 *   @arg       [17]   , ADC1_ETRGINJ_REMAP; [18]   , ADC1_ETRGREG_REMAP; [19]   , ADC2_ETRGINJ_REMAP;  [20]   , ADC2_ETRGREG_REMAP;
 *   @arg       [26:24], SWJ_CFG;
 * @param       bit: 占用多少位, 1 ~ 3, 详见pos参数说明
 * @param       val: 要设置的复用功能, 0 ~ 4, 得根据pos位数决定, 详细的设置值, 参见: <<STM32中文参考手册 V10>> 8.4.2节, 对MAPR寄存器的说明
 *              如: sys_gpio_remap_set(24, 3, 2); 则是设置SWJ_CFG[2:0]    = 2, 选择关闭JTAG, 开启SWD.
 *                  sys_gpio_remap_set(10, 2, 2); 则是设置TIM3_REMAP[1:0] = 2, TIM3选择部分重映射, CH1->PB4, CH2->PB5, CH3->PB0, CH4->PB1
 * @retval      无
 */
void sys_gpio_remap_set(uint8_t pos, uint8_t bit, uint8_t val)
{
    uint32_t temp = 0;
    uint8_t i = 0;
    RCC->APB2ENR |= 1 << 0;     /* 开启辅助时钟 */

    for (i = 0; i < bit; i++)   /* 填充bit个1 */
    {
        temp <<= 1;
        temp += 1;
    }

    AFIO->MAPR &= ~(temp << pos);       /* 清除MAPR对应位置原来的设置 */
    AFIO->MAPR |= (uint32_t)val << pos; /* 设置MAPR对应位置的值 */
}
/**
 * @brief       系统时钟初始化函数
 * @param       plln: PLL倍频系数(PLL倍频), 取值范围: 2~16
                中断向量表位置在启动时已经在SystemInit()中初始化
 * @retval      无
 */
void sys_stm32_clock_init(uint32_t plln)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
		
    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;       /* 选择要配置HSE */
    rcc_osc_init.HSEState = RCC_HSE_ON;                         /* 打开HSE */
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV2;          /* HSE预分频系数 */
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;                     /* 打开PLL */
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;             /* PLL时钟源选择HSE */
    rcc_osc_init.PLL.PLLMUL = plln;                             /* PLL倍频系数 */

    ret = HAL_RCC_OscConfig(&rcc_osc_init);                     /* 初始化 */

    if (ret != HAL_OK)
    {
        while (1);                                              /* 时钟初始化失败，之后的程序将可能无法正常执行，可以在这里加入自己的处理 */
    }
 
		
    /* 选中PLL作为系统时钟源并且配置HCLK,PCLK1和PCLK2*/
    rcc_clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;        /* 设置系统时钟来自PLL */
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;               /* AHB分频系数为1 */
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV2;                /* APB1分频系数为2 */
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;                /* APB2分频系数为1 */
		
    ret = HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_3);  /* 同时设置FLASH延时周期为2WS，也就是3个CPU周期。 */

    if (ret != HAL_OK)
    {
        while (1);                                              /* 时钟初始化失败，之后的程序将可能无法正常执行，可以在这里加入自己的处理 */
    }
		 __HAL_RCC_USB_CONFIG(RCC_USBCLK_DIV2_5);
}


void sys_gpio_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint32_t mode, uint32_t otype, uint32_t ospeed, uint32_t pupd)
{
    uint32_t pinpos = 0, pos = 0, curpin = 0;
    uint32_t config = 0;        /* 用于保存某一个IO的设置(CNF[1:0] + MODE[1:0]),只用了其最低4位 */

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* 一个个位检查 */
        curpin = pinx & pos;    /* 检查引脚是否要设置 */

        if (curpin == pos)      /* 需要设置 */
        {
            config = 0;         /* bit0~3都设置为0, 即CNF[1:0] = 0; MODE[1:0] = 0;  默认是模拟输入模式 */

            if ((mode == 0X01) || (mode == 0X02))   /* 如果是普通输出模式/复用功能模式 */
            {
                config = ospeed & 0X03;             /* 设置bit0/1 MODE[1:0] = 2/1/3 速度参数 */
                config |= (otype & 0X01) << 2;      /* 设置bit2   CNF[0]    = 0/1   推挽/开漏输出 */
                config |= (mode - 1) << 3;          /* 设置bit3   CNF[1]    = 0/1   普通/复用输出 */
            }
            else if (mode == 0)     /* 如果是普通输入模式 */
            {
                if (pupd == 0)   /* 不带上下拉,即浮空输入模式 */
                {
                    config = 1 << 2;               /* 设置bit2/3 CNF[1:0] = 01   浮空输入模式 */
                }
                else
                {
                    config = 1 << 3;                            /* 设置bit2/3 CNF[1:0] = 10   上下拉输入模式 */
                    p_gpiox->ODR &= ~(1 << pinpos);             /* 清除原来的设置 */
                    p_gpiox->ODR |= (pupd & 0X01) << pinpos;    /* 设置ODR = 0/1 下拉/上拉 */
                }
            }

            /* 根据IO口位置 设置CRL / CRH寄存器 */
            if (pinpos <= 7)
            {
                p_gpiox->CRL &= ~(0X0F << (pinpos * 4));        /* 清除原来的设置 */
                p_gpiox->CRL |= config << (pinpos * 4);         /* 设置CNFx[1:0] 和 MODEx[1:0], x = pinpos = 0~7 */
            }
            else
            {
                p_gpiox->CRH &= ~(0X0F << ((pinpos - 8) * 4));  /* 清除原来的设置 */
                p_gpiox->CRH |= config << ((pinpos - 8) * 4);   /* 设置CNFx[1:0] 和 MODEx[1:0], x = pinpos = 8~15 */

            }
        }
    }
}
