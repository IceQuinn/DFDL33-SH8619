#include "int_flash_mgmt.h"
#include <string.h>
#include "my_printf.h"
//#include "iwdg.h"
#include "my_drv.h"

#define STM32F4XX	0
#define STM32F1XX	1


#if STM32
#if STM32F4XX
uint32_t sectStartAddr[13][2]=
{
    {0x08000000, 16 },//0
    {0x08004000, 16 },//1
    {0x08008000, 16 },//2
    {0x0800c000, 16 },//3
    {0x08010000, 64 },//4
    {0x08020000, 128},//5
    {0x08040000, 128},//6
    {0x08060000, 128},//7
    {0x08080000, 128},//8
    {0x080a0000, 128},//9
    {0x080c0000, 128},//10
    {0x080e0000, 128},//11
    {0x08100000, 128},//12
};


// 擦除FLASH
void Flash_Erase(void)
{
    static FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase       = FLASH_TYPEERASE_SECTORS;	//闪存类型擦除扇区
    EraseInitStruct.Sector          = FLASH_SECTOR_2;	//闪存扇区2
    EraseInitStruct.NbSectors       = 6;				//扇区个数
    EraseInitStruct.VoltageRange    = VOLTAGE_RANGE_3;
    uint32_t SectorError = 0;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
   /* 给FLASH上锁，防止内容被篡改*/
   HAL_FLASH_Lock();
}
#endif

#if STM32F1XX

#define START_ERASE_PAGE_ADDR 0x08008000
#define PAGE_SIZE			  2048
#define PAGE_NUM			  10


// 擦除FLASH
void Flash_Erase(void)
{
	int i = 0;
	int cnt = 0;
    static FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase       = FLASH_TYPEERASE_PAGES;	//闪存类型擦除扇区
	for(i = 0; i < 240; i += PAGE_NUM)
	{
		HAL_IWDG_Refresh(&hiwdg);
		EraseInitStruct.PageAddress     = START_ERASE_PAGE_ADDR + (i*PAGE_SIZE);	//页起始地址
		EraseInitStruct.NbPages       	= PAGE_NUM;				//页数
		uint32_t SectorError = 0;

		HAL_FLASH_Unlock();
		HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
		/* 给FLASH上锁，防止内容被篡改*/
		HAL_FLASH_Lock();
		cnt++;
	}
//	MyPrintf("Flash_Erase_cnt = %d\r\n", cnt);
}
#endif


void Flash_Write(uint32_t addr, uint8_t *data, uint32_t data_size)
{
		if(data == NULL || data_size == 0)
			return ;

		HAL_FLASH_Unlock();
		uint32_t len = 4;
		
		for(uint32_t i = 0; i < data_size; i += len)
		{				
			uint32_t remaining = data_size - i;
			uint64_t data64 = 0;
				
			if(remaining >= 4 && (addr % 4) == 0)
				len = 4;
			else if(remaining >= 2 && (addr % 2) == 0)
				len = 2;
#if STM32F4XX
			else
				len = 1;			
#endif				
			memcpy(&data64, data + i, len);
				
			HAL_StatusTypeDef status;
			switch (len)
			{
#if STM32F4XX
			case 1:
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, data64);
					break;
#endif
			case 2:
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data64);
					break;
			case 4:
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data64);
					break;
			case 8:
					status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, data64);
					break;   
			default:
					status = HAL_ERROR;
					break;
			}
			if(status != HAL_OK)
			{
				MyPrintf("Write Failed At %#x! HAL Status: %d\r\n", addr, status);
				MyPrintf("Write Failed!!!\r\n");
				HAL_FLASH_Lock();
				return ;			//写入失败
			}
				
			//验证写入数据
			uint64_t read_data;
			switch(len)
			{
#if STM32F4XX
			case 1: 
				read_data = *(volatile uint8_t*)addr;
				break;
#endif
			case 2: 
				read_data = *(volatile uint16_t*)addr;
				break;
			case 4: 
				read_data = *(volatile uint32_t*)addr; 
				break;
			case 8:
				read_data = *(volatile uint64_t*)addr; 
				break;
			default: 
				break;
			}
			if(read_data != data64)
			{
				MyPrintf("Verification Failed!!!\r\n");
				HAL_FLASH_Lock();
				return ;			//验证失败
			}					
			addr += len;					
		}
		
    HAL_FLASH_Lock();
}

#endif

#if AT32

static uint32_t get_page(uint32_t addr)
{
    uint32_t page = 0;
    page = RT_ALIGN_DOWN(addr, FLASH_PAGE_SIZE);

    return page;
}

uint32_t at32_flash_write(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    ReturnTypeDef result = R_OK;
    uint32_t end_addr = addr + size - 1;
    uint32_t bytes_written = 0;

    if ((end_addr) > FLASH_BANK2_END_ADDR)
    {
        MyPrintf("write outrange flash size! addr is (0x%x)\r\n", (void *)(addr + size));
        return R_EINVAL;
    }

    flash_unlock();

    while (bytes_written < size)
    {
        uint32_t current_addr = addr + bytes_written;
        uint32_t remaining = size - bytes_written;
        const uint8_t *current_buf = buf + bytes_written;
        
        flash_status_type write_result = FLASH_OPERATE_DONE;
        
        // 优先按字写入（4字节对齐且剩余数据足够）
        if ((current_addr % 4 == 0) && (remaining >= 4))
        {
            uint32_t word_data = *((uint32_t *)current_buf);
            write_result = flash_word_program(current_addr, word_data);
            
            if (write_result == FLASH_OPERATE_DONE)
            {
                // 验证写入
                if (*(uint32_t *)current_addr != word_data)
                {
                    result = R_ERROR;
                    break;
                }
                bytes_written += 4;
            }
        }
        // 其次按半字写入（2字节对齐且剩余数据足够）
        else if ((current_addr % 2 == 0) && (remaining >= 2))
        {
            uint16_t halfword_data = *((uint16_t *)current_buf);         
            write_result = flash_halfword_program(current_addr, halfword_data);
            
            if (write_result == FLASH_OPERATE_DONE)
            {
                if (*(uint16_t *)current_addr != halfword_data)
                {
                    result = R_ERROR;
                    break;
                }
                bytes_written += 2;
            }
        }
        // 最后按字节写入
        else
        {
            uint8_t byte_data = *current_buf;
            write_result = flash_byte_program(current_addr, byte_data);
            
            if (write_result == FLASH_OPERATE_DONE)
            {
                if (*(uint8_t *)current_addr != byte_data)
                {
                    result = R_ERROR;
                    break;
                }
                bytes_written += 1;
            }
        }
        
        // 检查写入结果
        if (write_result != FLASH_OPERATE_DONE)
        {
            MyPrintf("Flash write failed at address 0x%08x", current_addr);
            result = R_ERROR;
            break;
        }
    }

    flash_lock();

    if (result != R_OK)
    {
        return result;
    }

    //return bytes_written;  // 返回实际写入的字节数
	return R_OK;
}


uint32_t at32_flash_erase(uint32_t addr, uint32_t size)
{
    ReturnTypeDef result = R_OK;
    uint32_t end_addr = addr + size - 1;
    uint32_t page_addr = 0;

    flash_unlock();

    if ((end_addr) > FLASH_BANK2_END_ADDR)
    {
        MyPrintf("erase outrange flash size! addr is (0x%x)", (void *)(addr + size));
        return R_EINVAL;
    }

    while(addr < end_addr)
    {
		wdt_counter_reload();
        page_addr = get_page(addr);

        if(flash_sector_erase(page_addr) != FLASH_OPERATE_DONE)
        {
            result = R_ERROR;
            goto __exit;
        }

        addr += FLASH_PAGE_SIZE;
    }

    flash_lock();

__exit:
    if(result != R_OK)
    {
		MyPrintf("goto __exit\r\n");
        return result;
    }

    //return size;
	return R_OK;
}


#endif


