#include "my_drv.h"
//#include <string.h>
#include "wk_system.h"
//#include "my_printf.h"

uint32_t at32_tick;

/*****************************************串口**********************************************/

ReturnTypeDef UART_Transmit(usart_type* usart_x, const uint8_t *pData, uint16_t Size)
{
	int i = 0;
	const uint8_t *Tx_Data;
	
	if((pData == NULL) || (Size == 0U))
	{
		return R_ERROR;
	}
	Tx_Data = pData;
	
	while(i < Size)
	{
		while(usart_flag_get(usart_x, USART_TDBE_FLAG) == RESET);
//		usart_data_transmit(usart_x, *Tx_Data);
		usart_data_transmit(usart_x, Tx_Data[i++]);
		while(usart_flag_get(usart_x, USART_TDC_FLAG) == RESET);
//		Tx_Data++;
	}
	
	return R_OK;
}


/*****************************************DMA**********************************************/

//void DMA_Restart(UART_StateType *uart_type)
//{
//	uint8_t* temp_addr;
//	
//	dma_channel_enable(uart_type->dmax_channely, FALSE);
////		if(uart_type->Head_pack_idx >= 60)
////		{
////			uart_type->Head_pack_idx = 0;
////			memset(uart_type->Uart_Rx_Idle_Buf, 0, sizeof(uart_type->Uart_Rx_Idle_Buf));
////		}
//	temp_addr = &(uart_type->Uart_Rx_Idle_Buf[uart_type->Head_pack_idx]);
//	uart_type->dmax_channely->maddr = (uint32_t)temp_addr;
//	uart_type->dmax_channely->dtcnt = countof(uart_type->Uart_Rx_Idle_Buf) - uart_type->Head_pack_idx;
//	uart_type->DMA_Dtcnt = uart_type->dmax_channely->dtcnt;
//	dma_channel_enable(uart_type->dmax_channely, TRUE);
//}	


/*****************************************定时器**********************************************/

//ReturnTypeDef TIM_Start(tmr_type *tmr_x)
//{
//    /* 检查定时器是否已经启动 */
//    
//    tmr_flag_clear(tmr_x, TMR_OVF_FLAG | TMR_TRIGGER_FLAG);   
//    tmr_interrupt_enable(tmr_x, TMR_OVF_INT, TRUE);
//    tmr_counter_enable(tmr_x, TRUE);
//    
//    return R_OK;
//}

//void TIM_Stop(tmr_type *tmr_x)
//{
//    tmr_counter_enable(tmr_x, FALSE);
//    tmr_interrupt_enable(tmr_x, TMR_OVF_INT, FALSE);

//    return ;
//}


//void TMR3_GLOBAL_IRQHandler(void)
//{
//	if (tmr_flag_get(TMR3, TMR_OVF_FLAG) == SET)
//    {
//        tmr_flag_clear(TMR3, TMR_OVF_FLAG);
//		at32_tick++;
//    }
//}

uint32_t AT32_GetTick(void)
{
	return at32_tick;
}


/*****************************************SPI**********************************************/

ReturnTypeDef AT32_SPI_Transmit(spi_type* spi_x, const uint8_t *pData, uint16_t Size)
{
	int i = 0;
	const uint8_t *Tx_Data;
	uint8_t data;
	
	if((pData == NULL) || (Size == 0U))
	{
		return R_ERROR;
	}
	Tx_Data = pData;
	
	while(i < Size)
	{
		while(spi_i2s_flag_get(spi_x, SPI_I2S_TDBE_FLAG) == RESET);
		spi_i2s_data_transmit(spi_x, Tx_Data[i++]);
		while(spi_i2s_flag_get(spi_x, SPI_I2S_RDBF_FLAG) == RESET);
		data = spi_i2s_data_receive(spi_x);
//		while(spi_i2s_flag_get(spi_x, SPI_I2S_TDBE_FLAG) == RESET);
	}
//	while(spi_i2s_flag_get(spi_x, SPI_I2S_BF_FLAG) == RESET);
	
	return R_OK;
}

ReturnTypeDef AT32_SPI_Receive(spi_type* spi_x, uint8_t *pData, uint16_t Size)
{
	int i = 0;
	int data = 0xff;
	uint8_t *Rx_Data;
	
	spi_i2s_flag_clear(spi_x, SPI_I2S_ROERR_FLAG);
	
	if((pData == NULL) || (Size == 0U))
	{
		return R_ERROR;
	}
	Rx_Data = pData;
	
	while(i < Size)
	{
		while(spi_i2s_flag_get(spi_x, SPI_I2S_TDBE_FLAG) == RESET);
		spi_i2s_data_transmit(spi_x, data);
		while(spi_i2s_flag_get(spi_x, SPI_I2S_RDBF_FLAG) == RESET);
		Rx_Data[i++] = spi_i2s_data_receive(spi_x);
	}
//	while(spi_i2s_flag_get(spi_x, SPI_I2S_BF_FLAG) == RESET);
	
	return R_OK;
}

ReturnTypeDef AT32_SPI_TransmitReceive(spi_type* spi_x, const uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
	return R_OK;
}


/*****************************************FLASH**********************************************/



