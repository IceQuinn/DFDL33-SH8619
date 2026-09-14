#ifndef __MY_DRV_H__
#define __MY_DRV_H__

#include "wk_usart.h"

#define IDLE_EVENT_BUFFER_SIZE 1024

#ifndef countof
#define countof(x)      (sizeof(x)/sizeof(x[0]))
#endif

typedef enum
{
	R_OK		= 0x00U,
	R_ERROR		= 0x01U,
	R_BUSY		= 0x02U,
	R_TIMEOUT	= 0x03U,
	R_EINVAL	= 0x04U
}ReturnTypeDef;

ReturnTypeDef UART_Transmit(usart_type* usart_x, const uint8_t *pData, uint16_t Size);

//void DMA_Restart(UART_StateType *uart_type);

//ReturnTypeDef TIM_Start(tmr_type *tmr_x);
//void TIM_Stop(tmr_type *tmr_x);
uint32_t AT32_GetTick(void);

ReturnTypeDef AT32_SPI_Transmit(spi_type* spi_x, const uint8_t *pData, uint16_t Size);
ReturnTypeDef AT32_SPI_Receive(spi_type* spi_x, uint8_t *pData, uint16_t Size);
ReturnTypeDef AT32_SPI_TransmitReceive(spi_type* spi_x, const uint8_t *pTxData, uint8_t *pRxData, uint16_t Size);


#endif
