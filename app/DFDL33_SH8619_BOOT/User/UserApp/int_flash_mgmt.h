#ifndef __INT_FLASH_MGMT_H__
#define __INT_FLASH_MGMT_H__

#include <stdint.h>

#define STM32		0
#define AT32		1

#if STM32
void Flash_Erase(void);
void Flash_Write(uint32_t addr, uint8_t *data, uint32_t data_size);
#endif

#if AT32
#define	FLASH_TYPEPROGRAM_BYTE			8
#define FLASH_TYPEPROGRAM_HALFWORD		16
#define	FLASH_TYPEPROGRAM_WORD			32

#define FLASH_PAGE_SIZE					(2 * 1024)
#define RT_ALIGN_DOWN(size, align)  	((size) & ~((align) - 1))

uint32_t at32_flash_write(uint32_t addr, const uint8_t *buf, uint32_t size);
uint32_t at32_flash_erase(uint32_t addr, uint32_t size);
#endif

#endif 

