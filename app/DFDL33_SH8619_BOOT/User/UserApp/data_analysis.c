#include "data_analysis.h"
#include "int_flash_mgmt.h"
#include "jumpAPP.h"
#include "crc32.h"
#include "crc8.h"
#include "drv_zd25q64.h"
#include "wk_system.h"
#include "wk_wdt.h"
#include "crc16.h"

#define EXTFLASH_PAGE_SIZE      (4096)
#define HAND_PACK_ADDDR         (1049 * EXTFLASH_PAGE_SIZE)         //摘要数据写入外部flash的起始地址
#define START_ADDR 				(1050 * EXTFLASH_PAGE_SIZE) 		//APP写入外部flash的起始地址
//#define WRITE_ADDR				(1042 * EXTFLASH_PAGE_SIZE)			//BOOT+APP写入外部flash的起始地址
#define APP_START_ADDR			(0x08008000)						//APP起始地址
#define FLASH_ERASE				(496 * FLASH_PAGE_SIZE)				//擦除扇区大小

struct IAP_Hand_Pack_Str  IAP_Hand_Pack;							//摘要帧
//static uint32_t sectstartaddr = APP_START_ADDR;						//写入内部flash地址


/*****************************校验帧处理*****************************/

int Check_Pack_Deal(void)
{
	uint32_t crc32_r_cal = 0;
	uint32_t bytes_read = 0;
	uint32_t readaddr   = START_ADDR;
	uint8_t  updata_buf[4096];
		
//		MyPrintf("Start_Check = %08d\r\n", AT32_GetTick());
	MyPrintf("Start_Check\r\n");
		while (bytes_read < IAP_Hand_Pack.File_Size) 
		{
			//未读取剩余数量
			uint32_t remaining = IAP_Hand_Pack.File_Size - bytes_read;
			uint16_t read_size = (remaining > EXTFLASH_PAGE_SIZE) ? EXTFLASH_PAGE_SIZE : remaining;
			flash_read(readaddr, updata_buf, read_size);				
			crc32_r_cal = crc32(updata_buf, read_size, &crc32_r_cal);
				
			readaddr   += read_size;
			//已读取数量
			bytes_read += read_size;
//			wk_delay_ms(10);
		}
		
		//读取完毕
		if(bytes_read == IAP_Hand_Pack.File_Size)
		{
			crc32_r_cal = crc32_r_cal ^ 0xffffffff;
//			MyPrintf("[%08d]full_crc32_r_cal = %x\r\n", AT32_GetTick(), crc32_r_cal);
			MyPrintf("full_crc32_r_cal = %x\r\n", crc32_r_cal);
		}
//		MyPrintf("Stop_Check = %08d\r\n", AT32_GetTick());
		MyPrintf("Stop_Check\r\n");
		
		//校验结果判断
		if(crc32_r_cal == IAP_Hand_Pack.File_CRC)
		{
//			MyPrintf("[%08d]Check Success\r\n", AT32_GetTick());
			MyPrintf("Check Success\r\n");
			Write_Flash_Deal();
		}
		else
		{
//			MyPrintf("[%08d]Check Failed\r\n", AT32_GetTick());
			MyPrintf("Check Failed\r\n");
			return -1;
		}
	
		return 0;
}


/*****************************写flash处理*****************************/

void Write_Flash_Deal(void)
{
	static uint32_t sectstartaddr = APP_START_ADDR;						//写入内部flash地址
	
	wdt_counter_reload();
	if(IAP_Hand_Pack.File_Size > FLASH_ERASE)
	{
		MyPrintf("File too large!\r\n");
		return;
	}
//	MyPrintf("[%08d]Flash_Erase Start\r\n", AT32_GetTick());
	MyPrintf("Flash_Erase Start\r\n");
	at32_flash_erase(APP_START_ADDR, FLASH_ERASE);  //擦除
//	MyPrintf("[%08d]Flash_Erase End\r\n", AT32_GetTick());
	MyPrintf("Flash_Erase End\r\n");	
	uint32_t bytes_read = 0;
	uint32_t readaddr   = START_ADDR;
	uint8_t  updata_buf[4096];
	
//	MyPrintf("[%08d]Write Flash Start\r\n", AT32_GetTick());
	MyPrintf("Write Flash Start\r\n");
	while (bytes_read < IAP_Hand_Pack.File_Size) 
	{
		wdt_counter_reload();
		//未读取剩余数量
        uint32_t remaining = IAP_Hand_Pack.File_Size - bytes_read;
        uint16_t read_size = (remaining > EXTFLASH_PAGE_SIZE) ? EXTFLASH_PAGE_SIZE : remaining;
        flash_read(readaddr, updata_buf, read_size);				
			
		at32_flash_write(sectstartaddr, updata_buf, read_size);	
        readaddr += read_size;
		sectstartaddr += read_size;
		//已读取数量
        bytes_read += read_size;
    }
//	MyPrintf("[%08d]Write Flash Stop\r\n", AT32_GetTick());
	MyPrintf("Write Flash Stop\r\n");
	
	if(bytes_read == IAP_Hand_Pack.File_Size)
	{
		//升级成功，擦除摘要
		Flash_Erase_4K(HAND_PACK_ADDDR);
//		MyPrintf("[%08d]Update Successfully, JumpToApp\r\n", AT32_GetTick());
		MyPrintf("Update Successfully, JumpToApp\r\n");
		
		wdt_counter_reload();
		JumpToApp();
	}
	else
	{
//		MyPrintf("[%08d]Flash Data Read Failed!!!", AT32_GetTick());
		MyPrintf("Flash Data Read Failed!!!");
	}
	return ;
}

void Upgrade_Check(void)
{
	uint16_t hand_pack_crc16 = 0;
	flash_read(HAND_PACK_ADDDR, &IAP_Hand_Pack, sizeof(struct IAP_Hand_Pack_Str));
	
	hand_pack_crc16 = crc16(&IAP_Hand_Pack, sizeof(struct IAP_Hand_Pack_Str)-2);

	if(hand_pack_crc16 != IAP_Hand_Pack.Hand_CRC16)
	{
		MyPrintf("Hand Pack Check Failed, Jump To App\r\n");
		JumpToApp();
	}
	else{
		//判断是否需要升级
		if(1 != IAP_Hand_Pack.Upgrade_Flag)
		{
			MyPrintf("No need for an upgrade\r\n");
			JumpToApp();
		}
		else
		{
			if(-1 == Check_Pack_Deal())
			{
				MyPrintf("Upgrade package verification failed, jump to App\r\n");
				JumpToApp();
			}
		}
	}
}





