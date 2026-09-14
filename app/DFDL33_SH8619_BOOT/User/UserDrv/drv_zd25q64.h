#ifndef __DRV_W25Q64_H__
#define __DRV_W25Q64_H__

#include "spi_flash.h"

#define SPI_FLASH_CS_LOW()      gpio_bits_reset(GPIOB, GPIO_PINS_12)
#define SPI_FLASH_CS_HIGH()     gpio_bits_set(GPIOB, GPIO_PINS_12)

#define FLASH_CMD_PAGE_PROGRAM  0x02
#define FLASH_CMD_READ_DATA     0x03
#define FLASH_CMD_READ_REGISTER 0x05
#define FLASH_CMD_WRITE_ENABLE  0x06
#define FLASH_CMD_ERASE_4K      0x20
#define FLASH_CMD_CHIP_ERASE    0xC7

void Check_GD25Q64(void);

//¼Ä´æÆ÷×´Ì¬²éÑ¯
uint8_t Flash_Read_Register(void);

int Flash_Write_Enable(void);

void Flash_Chip_Erase(void);

int Flash_Erase_4K(uint32_t flashaddr);

int Flash_Page_Write(uint32_t flashaddr, void* data, uint32_t len);

int flash_write(uint32_t flashaddr, void* data, uint32_t len);

int flash_read(uint32_t flashaddr, void* data, uint32_t len);

#endif
