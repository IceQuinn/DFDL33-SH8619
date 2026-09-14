#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include <stdint.h>
#include "wk_spi.h"

void SPI_Transmit(void *pData, uint16_t Size);
void SPI_Receive(void *pData, uint16_t Size);
void SPI_TransmitReceive(void *pTxData, void *pRxData, uint16_t Size);

#endif
