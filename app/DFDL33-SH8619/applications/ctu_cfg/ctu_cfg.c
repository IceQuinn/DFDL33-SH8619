/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-14     mutou       the first version
 */
#define DBG_TAG "ctu_cfg"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <string.h>

#include "crc16.h"
#include <math.h>
#include "ctu_cfg.h"
#include "AB_check.h"
#include "sys.h"
#include "user_ex_flash_mgmt.h"
#include "drv_ex_flash.h"

// 本地全局变量
GSE8625_CfgTypeDef_Vlast ctu_cfg;

void Ctu_Cfg_Init(void)
{
    int check_sta = 0;
    check_sta = AB_check(flash_read,       //读接口
                    flash_write,           //写接口
                    CTU_CFG_ADDR_A,         //A区地址
                    CTU_CFG_ADDR_B,         //B区地址
                    &ctu_cfg,               //数据内存
                    sizeof(ctu_cfg),        //数据大小
                    "FLASH CTU CFG");      //描述
    if(1 == check_sta)
    {
        // check_save_sta.check_para_sta = 1;
        // Send_Event_To_Cache(ETP_PARA_CHECK_ERROR, EVT_SET, RT_NULL);
        set_default_data();
    }
    else
    {
        // 版本升级
//        CFG_Vx_To_Vlast(ctu_cfg.hdr.ver, ctu_cfg.hdr.len);
//        ctu_cfg_save();
    }
    set_default_data();
}

void set_default_para(void)
{
    // RS485-2
    ctu_cfg.uart_protocol[UART1_NO]     = MODBUS_MASTER;//通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART1_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART1_NO]        = 1;            //校验位
    // RJ45-1-1
    ctu_cfg.uart_protocol[UART3_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART3_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART3_NO]        = 1;            //校验位

    // RJ45-1-2
    ctu_cfg.uart_protocol[UART4_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART4_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART4_NO]        = 1;            //校验位

    // RJ45-2-1
    ctu_cfg.uart_protocol[UART5_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART5_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART5_NO]        = 1;            //校验位

    // RJ45-2-2
    ctu_cfg.uart_protocol[UART6_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART6_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART6_NO]        = 1;            //校验位

    // RS485-1
    ctu_cfg.uart_protocol[UART7_NO]     = DLT645_SLAVE; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART7_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART7_NO]        = 3;            //校验位

    // 载波
    ctu_cfg.uart_protocol[UART8_NO]     = DLT645_SLAVE; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART8_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART8_NO]        = 3;            //校验位

    for(uint8_t i=0; i<6; i++)
    {
        ctu_cfg.dlt645_bcd_addr[i]      = i;        //dlt645通信地址
    }

    ctu_cfg.longitude                   = 1143999;      //经度
    ctu_cfg.latitude                    = 304456;       //纬度

}

//恢复默认值并保存
void set_default_data(void)
{
    // Send_Event_To_Cache(ETP_RES_DEFAULT_PARA, EVT_SET, RT_NULL);
    set_default_para();
    ctu_cfg_save();
}
MSH_CMD_EXPORT(set_default_data, set_default_data);

void ctu_cfg_save(void)
{
    if(is_power_down() && !is_power_down_legal_work()){
        LOG_E("Power Down ctu_cfg_save Quit");
        return ;
    }

    AB_save(flash_write, CTU_CFG_ADDR_A, CTU_CFG_ADDR_B, &ctu_cfg, CTU_CFG_VER, sizeof(ctu_cfg), "EEPROM CTU CFG");
}
MSH_CMD_EXPORT(ctu_cfg_save, ctu_cfg_save);


uint32_t baud_table[][2] = {{0, 2400}, {1, 4800}, {2, 9600}, {3, 19200}, {4, 38400}};

int baud_check(uint32_t baud)
{
    for (uint16_t i=0; i < countof(baud_table); ++i)
    {
        if(baud_table[i][1] == baud)
        {
            return 0;
        }
    }
    return 1;
}
