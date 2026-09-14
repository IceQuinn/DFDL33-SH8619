#ifndef __JUMPAPP_H__
#define __JUMPAPP_H__

#define STM32	0
#define AT32	1

#if	STM32
#include "usart.h"

void GPIO_DeInit(GPIO_TypeDef  *GPIOx);
void UART_DeInit(UART_HandleTypeDef *huart);
void TIM_DeInit(TIM_TypeDef *TIMx);
void SPI_DeInit(SPI_TypeDef *SPIx);
#endif

#if AT32
#include "wk_gpio.h"

void GPIO_DeInit(gpio_type *gpio_x);
//void UART_DeInit(usart_type* usart_x);
//void TIM_DeInit(tmr_type *tmr_x);
void SPI_DeInit(spi_type* spi_x);

#endif

void JumpToApp(void);

#endif
