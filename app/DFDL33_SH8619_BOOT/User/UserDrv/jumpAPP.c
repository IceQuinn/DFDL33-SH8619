#include "jumpAPP.h"
#include <stdint.h>
#include "my_printf.h"
#include "wk_system.h"

#define APP_START_ADDR		(0x08008000)			//APP起始地址

typedef void (*BootToApp)(void);		

#if STM32
void GPIO_DeInit(GPIO_TypeDef  *GPIOx)
{
	if(GPIOx == GPIOA)
	{
		__HAL_RCC_GPIOA_CLK_DISABLE();
		__HAL_RCC_GPIOA_FORCE_RESET();
		__HAL_RCC_GPIOA_RELEASE_RESET();
	}
	else if(GPIOx == GPIOB)
	{
		__HAL_RCC_GPIOB_CLK_DISABLE();
		__HAL_RCC_GPIOB_FORCE_RESET();
		__HAL_RCC_GPIOB_RELEASE_RESET();
	}
	else if(GPIOx == GPIOC)
	{
		__HAL_RCC_GPIOC_CLK_DISABLE();
		__HAL_RCC_GPIOC_FORCE_RESET();
		__HAL_RCC_GPIOC_RELEASE_RESET();
	}
	else if(GPIOx == GPIOD)
	{
		__HAL_RCC_GPIOD_CLK_DISABLE();
		__HAL_RCC_GPIOD_FORCE_RESET();
		__HAL_RCC_GPIOD_RELEASE_RESET();
	}
	
	return ;	
}

void UART_DeInit(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1)
	{
		__HAL_RCC_USART1_FORCE_RESET();
		__HAL_RCC_USART1_RELEASE_RESET();
	}
	else if(huart->Instance == USART2)
	{
		__HAL_RCC_USART2_FORCE_RESET();
		__HAL_RCC_USART2_RELEASE_RESET();
	}
	else if(huart->Instance == UART4)
	{
		__HAL_RCC_UART4_FORCE_RESET();
		__HAL_RCC_UART4_RELEASE_RESET();
	}
	else if(huart->Instance == UART5)
	{
		__HAL_RCC_UART5_FORCE_RESET();
		__HAL_RCC_UART5_RELEASE_RESET();
	}

	return ;
}

void TIM_DeInit(TIM_TypeDef *TIMx)
{
	if (TIMx == TIM2)
	{
		__HAL_RCC_TIM2_CLK_DISABLE();
		__HAL_RCC_TIM2_FORCE_RESET();
		__HAL_RCC_TIM2_RELEASE_RESET();
	}
	else if(TIMx == TIM3)
	{
		__HAL_RCC_TIM3_CLK_DISABLE();
		__HAL_RCC_TIM3_FORCE_RESET();
		__HAL_RCC_TIM3_RELEASE_RESET();	
	}
	else if(TIMx == TIM4)
	{
		__HAL_RCC_TIM4_CLK_DISABLE();
		__HAL_RCC_TIM4_FORCE_RESET();
		__HAL_RCC_TIM4_RELEASE_RESET();	
	}
	
	return ;
}

void SPI_DeInit(SPI_TypeDef *SPIx)
{
	if(SPIx == SPI2)
	{
		__HAL_RCC_SPI1_CLK_DISABLE();
		__HAL_RCC_SPI1_FORCE_RESET();
		__HAL_RCC_SPI1_RELEASE_RESET();
	}
//	else if(SPIx == SPI2)
//	{
//		__HAL_RCC_SPI2_CLK_DISABLE();
//		__HAL_RCC_SPI2_FORCE_RESET();
//		__HAL_RCC_SPI2_RELEASE_RESET();
//	}
	
	return ;
}

#endif

#if AT32
void GPIO_DeInit(gpio_type *gpio_x)
{
	if(GPIOA == gpio_x)
	{
		crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, FALSE);
	}
	else if(GPIOB == gpio_x)
	{
		crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, FALSE);
	}
	else if(GPIOC == gpio_x)
	{
		crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, FALSE);
	}
	else if(GPIOD == gpio_x)
	{
		crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, FALSE);
	}
//	else if(GPIOH == gpio_x)
//	{
//		crm_periph_clock_enable(CRM_GPIOH_PERIPH_CLOCK, FALSE);
//	}
	gpio_reset(gpio_x);
	
	return ;	
}

void UART_DeInit(usart_type* usart_x)
{
	usart_enable(usart_x, FALSE);
	usart_interrupt_enable(usart_x, USART_IDLE_INT, FALSE);
//	usart_dma_receiver_enable(usart_x, FALSE);
//	
//	usart_transmitter_enable(usart_x, FALSE);
//	usart_receiver_enable(usart_x, FALSE);
	
	if (USART1 == usart_x)
	{
		nvic_irq_disable(USART1_IRQn);
		crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, FALSE);
	}
	else if(USART2 == usart_x)
	{
		crm_periph_clock_enable(CRM_USART2_PERIPH_CLOCK, FALSE);
	}
	else if(USART3 == usart_x)
	{
		nvic_irq_disable(USART3_IRQn);
		crm_periph_clock_enable(CRM_USART3_PERIPH_CLOCK, FALSE);
	}
	else if(USART6 == usart_x)
	{
		nvic_irq_disable(USART6_IRQn);
		crm_periph_clock_enable(CRM_USART6_PERIPH_CLOCK, FALSE);
	}
	
	usart_reset(usart_x);
	
	return ;
}

//void TIM_DeInit(tmr_type *tmr_x)
//{
//	tmr_counter_enable(tmr_x, FALSE);
//	if(TMR2 == tmr_x)
//	{		
//		nvic_irq_disable(TMR2_GLOBAL_IRQn);
//		crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, FALSE);
//		crm_periph_reset(CRM_TMR2_PERIPH_RESET, TRUE);
//		crm_periph_reset(CRM_TMR2_PERIPH_RESET, FALSE);
//	}
//	
//	
//	
//	return ;
//}

void SPI_DeInit(spi_type* spi_x)
{
	spi_enable(spi_x, FALSE);
	if(SPI2 == spi_x)
	{
		crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, FALSE);
	}
	spi_i2s_reset(spi_x);
	
	return ;
}

//void DMA_DeInit(dma_type *dma_x, dma_channel_type *dmax_channely)
//{
//	dma_channel_enable(dmax_channely, FALSE);
//	dmamux_enable(dma_x, FALSE);
//	dma_reset(dmax_channely);
//	crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, FALSE);
//	crm_periph_reset(CRM_DMA1_PERIPH_RESET, TRUE);
//	crm_periph_reset(CRM_DMA1_PERIPH_RESET, FALSE);
//	
//	return ;
//}

void wk_system_clock_deinit_before_jump(void)
{
    /* reset crm */
  crm_reset();

  /* enable pwc periph clock */
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, TRUE);

  /* config ldo voltage */
//  pwc_ldo_output_voltage_set(PWC_LDO_OUTPUT_1V2);
 
  /* set the flash clock divider */
	
//  flash_clock_divider_set(FLASH_CLOCK_DIV_4); //改

  /* enable battery powered domain access */
  pwc_battery_powered_domain_access(TRUE);

  /* check lext enabled or not */
  if(crm_flag_get(CRM_LEXT_STABLE_FLAG) == RESET)
  {
    crm_clock_source_enable(CRM_CLOCK_SOURCE_LEXT, TRUE);
    while(crm_flag_get(CRM_LEXT_STABLE_FLAG) == RESET)
    {
		MyPrintf("1\r\n");
    }
  }
  /* disable battery powered domain access */
  pwc_battery_powered_domain_access(FALSE);
  /* disable pwc periph clock */
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, FALSE);

  /* enable lick */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_LICK, FALSE);

  /* wait till lick is ready */
  while(crm_flag_get(CRM_LICK_STABLE_FLAG) != RESET)
  {
	  MyPrintf("2\r\n");
  }

  /* enable hext */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, FALSE);

  /* wait till hext is ready */
  while(crm_hext_stable_wait() == SUCCESS)
  {
	  MyPrintf("3\r\n");
  }

  /* enable hick */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, FALSE);

  /* wait till hick is ready */
  while(crm_flag_get(CRM_HICK_STABLE_FLAG) != SET)
  {
	  MyPrintf("4\r\n");
  }

  /* config pll clock resource */
//  crm_pll_config(CRM_PLL_SOURCE_HICK, 144, 1, CRM_PLL_FR_4);

  /* enable pll */
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, FALSE);

  /* wait till pll is ready */
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) != RESET)
  {
	  MyPrintf("5\r\n");
  }

  /* config ahbclk */
//  crm_ahb_div_set(CRM_AHB_DIV_1);

  /* config apb2clk, the maximum frequency of APB2 clock is 144 MHz  */
//  crm_apb2_div_set(CRM_APB2_DIV_2);

  /* config apb1clk, the maximum frequency of APB1 clock is 144 MHz  */
//  crm_apb1_div_set(CRM_APB1_DIV_2);

  /* enable auto step mode */
  crm_auto_step_mode_enable(TRUE);

  /* select pll as system clock source */
  crm_sysclk_switch(CRM_SCLK_PLL);

  /* wait till pll is used as system clock source */
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
  {
	  MyPrintf("6\r\n");
  }

  /* disable auto step mode */
  crm_auto_step_mode_enable(FALSE);

  /* update system_core_clock global variable */
  system_core_clock_update();
}

#endif

void JumpToApp(void) 
{	
    BootToApp app_start;

#if STM32
    // 关闭所有外设时钟和中断
	HAL_RCC_DeInit();

	GPIO_DeInit(GPIOA);
	GPIO_DeInit(GPIOB);
	GPIO_DeInit(GPIOC);
	GPIO_DeInit(GPIOD);
   
	UART_DeInit(&huart1);
	UART_DeInit(&huart2);
	UART_DeInit(&huart4);
	UART_DeInit(&huart5);
	
	TIM_DeInit(TIM2);
	
	//SPI_DeInit(SPI1);
	SPI_DeInit(SPI2);
#endif

#if AT32
//	DMA_DeInit(DMA1, DMA1_CHANNEL1);
//	DMA_DeInit(DMA1, DMA1_CHANNEL2);
//	DMA_DeInit(DMA1, DMA1_CHANNEL3);
//	
//	UART_DeInit(USART1);
	UART_DeInit(USART2);
//	UART_DeInit(USART3);
////	UART_DeInit(USART6);
	
//	TIM_DeInit(TMR2);
//	TIM_DeInit(TMR3);
	
	SPI_DeInit(SPI2);
	
	GPIO_DeInit(GPIOA);
	GPIO_DeInit(GPIOB);
	GPIO_DeInit(GPIOC);
//	GPIO_DeInit(GPIOH);
	
//	wk_system_clock_deinit_before_jump();
	crm_reset();
	
#endif

//    //验证栈顶地址合法性
//    uint32_t msp = *(__IO uint32_t*)APP_START_ADDR;
//    if ((msp & 0x2FFE0000) != 0x20000000) {
//        MyPrintf("Invalid MSP: %#08x\r\n", msp);
//        return;
//    }

    //设置向量表地址和主堆栈指针
    SCB->VTOR = APP_START_ADDR;     //必须与APP工程的SCB->VTOR一致
    __set_MSP( *(__IO uint32_t*)APP_START_ADDR);

    //获取APP入口地址并跳转
    app_start = (BootToApp)(*(__IO uint32_t*)(APP_START_ADDR + 4));
	
	//禁用并清除所有中断
    __disable_irq();
	NVIC->ICER[0] = 0xFFFFFFFF;		//禁用所有中断
	NVIC->ICPR[0] = 0xFFFFFFFF;		//清除所有挂起中断

    app_start();
    MyPrintf("Jump App Failed!!!\r\n");
}
