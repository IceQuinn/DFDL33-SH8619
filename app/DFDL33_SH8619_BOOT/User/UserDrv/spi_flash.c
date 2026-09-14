#include "spi_flash.h"
#include "my_drv.h"

void SPI_Transmit(void *pData, uint16_t Size)
{
	AT32_SPI_Transmit(SPI2, pData, Size);
}


void SPI_Receive(void *pData, uint16_t Size)
{
	AT32_SPI_Receive(SPI2, pData, Size);
}


void SPI_TransmitReceive(void *pTxData, void *pRxData, uint16_t Size)
{
	AT32_SPI_TransmitReceive(SPI2, pTxData, pRxData, Size);
}

