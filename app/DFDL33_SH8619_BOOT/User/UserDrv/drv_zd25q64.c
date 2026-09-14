#include "drv_zd25q64.h"
#include "my_printf.h"
#include "wk_system.h"

void Check_GD25Q64(void)
{
    uint8_t tx_buf[6] = {0x90, 0x00, 0x00, 0x00};
    uint8_t rx_buf[6] = {0};
		
    SPI_FLASH_CS_LOW();
    SPI_Transmit(tx_buf, 4);
    SPI_Receive(rx_buf, 2);
//    SPI_TransmitReceive(tx_buf, rx_buf, 6);
    SPI_FLASH_CS_HIGH();

    if((0xC8 == rx_buf[0]) && (0x16 == rx_buf[1]))
    {
        MyPrintf("SPI FLASH GD25Q64 Init Success\r\n");
    }
    else
    {
        MyPrintf("SPI FLASH GD25Q64 Init Failed\r\n");
    }
}


//¼Ä´æÆ÷×´Ì¬²éÑ¯
uint8_t Flash_Read_Register(void)
{
	Flash_Write_Enable();
	uint8_t tx_buf1 = FLASH_CMD_READ_REGISTER;
	uint8_t rx_buf1 = 0;
	SPI_FLASH_CS_LOW();
	SPI_Transmit(&tx_buf1, 1);
	SPI_Receive(&rx_buf1, 1);
	SPI_FLASH_CS_HIGH();

	return rx_buf1;
}


//Ð´Ê¹ÄÜ
int Flash_Write_Enable(void)
{
	uint8_t tx_buf = FLASH_CMD_WRITE_ENABLE;
	SPI_FLASH_CS_LOW();
	SPI_Transmit(&tx_buf, 1);
	SPI_FLASH_CS_HIGH();
	
	return 0;
}


//Ð¾Æ¬²Á³ý
void Flash_Chip_Erase(void)
{
	Flash_Write_Enable();
	uint8_t tx_buf = FLASH_CMD_CHIP_ERASE;
	SPI_FLASH_CS_LOW();
	SPI_Transmit(&tx_buf, 1);
	SPI_FLASH_CS_HIGH();
	
}


//²Á³ýÉÈÇø
int Flash_Erase_4K(uint32_t flashaddr)
{
	Flash_Write_Enable();
	uint8_t tx_buf[4] = {FLASH_CMD_ERASE_4K, 0x00, 0x00, 0x00};
	tx_buf[1] = flashaddr >> 16;
	tx_buf[2] = flashaddr >> 8;
	tx_buf[3] = flashaddr >> 0;
	SPI_FLASH_CS_LOW();
	SPI_Transmit(&tx_buf, 4);
	SPI_FLASH_CS_HIGH();
	
	while(1 == (Flash_Read_Register() & 0x01));
	
	return 0;
}


//Ò³±à³Ì
int Flash_Page_Write(uint32_t flashaddr, void* data, uint32_t len)
{
	Flash_Write_Enable();
	uint8_t tx_buf[4] = {FLASH_CMD_PAGE_PROGRAM, 0x00, 0x00, 0x00};
	tx_buf[1] = flashaddr >> 16;
	tx_buf[2] = flashaddr >> 8;
	tx_buf[3] = flashaddr >> 0;
	SPI_FLASH_CS_LOW();
	SPI_Transmit(&tx_buf, 4);
	SPI_Transmit(data, len);
	SPI_FLASH_CS_HIGH();
	
	while(1 == (Flash_Read_Register() & 0x01));
	
	return 0;
}


//Ð´flash
int flash_write(uint32_t flashaddr, void* data, uint32_t len)
{
	Flash_Erase_4K(flashaddr);	
	Flash_Write_Enable();

	uint8_t *p_Page_data = data;
	int i = 0;
	for(i = 0; i < (len / 256); ++i)
	{
		Flash_Page_Write((flashaddr + 256 * i), &p_Page_data[256 * i], 256);
		wk_delay_ms(10);
	}
	if(len % 256)
	{
		Flash_Page_Write((flashaddr + 256 * i), &p_Page_data[256 * i], len % 256);
		wk_delay_ms(10);
	}
  
	return 0;
}

//¶Áflash
int flash_read(uint32_t flashaddr, void* data, uint32_t len)
{
	uint8_t tx_buf[4] = {FLASH_CMD_READ_DATA, 0x00, 0x00, 0x00};
	tx_buf[1] = flashaddr >> 16;
	tx_buf[2] = flashaddr >> 8;
	tx_buf[3] = flashaddr >> 0;
	
	while(1 == (Flash_Read_Register() & 0x01));

	SPI_FLASH_CS_LOW();
	SPI_Transmit(tx_buf, 4);
	SPI_Receive(data, len);
	SPI_FLASH_CS_HIGH();
	
	return 0;
}

