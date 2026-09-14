#ifndef __DATA_ANALYSIS_H__
#define __DATA_ANALYSIS_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "my_printf.h"
#include "my_drv.h"


//摘要数据
struct IAP_Hand_Pack_Str
{
    uint8_t  Upgrade_Flag;      // 升级标志
    uint8_t  Upgrade_Type;      // 升级类型，1:Boot，2:App
    uint16_t Upgrade_Ver;       // 升级版本
    uint32_t File_Size;         // 程序总大小
    uint32_t File_CRC;          // 程序CRC32
    uint16_t Hand_CRC16;		// 帧CRC
}__attribute__((packed));


int Check_Pack_Deal(void);

void Write_Flash_Deal(void);
void Upgrade_Check(void);


#endif

