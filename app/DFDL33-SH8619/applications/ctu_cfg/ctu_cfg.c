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
#include <stddef.h>

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
    uint16_t saved_payload_len = 0U; /* 保存Flash记录的旧负载长度，用于逐个识别结构体尾部新增字段。 */
    rt_bool_t config_upgraded = RT_FALSE; /* 标识本次启动是否补充了旧配置中不存在的字段。 */
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
        saved_payload_len = ctu_cfg.hdr.len; /* 必须在重新保存前保留旧长度，避免后续字段兼容判断失真。 */
        if(saved_payload_len < (offsetof(GSE8625_CfgTypeDef_Vlast, altitude) + sizeof(ctu_cfg.altitude) - sizeof(ctu_cfg.hdr))) /* 旧版配置不含高度字段时补充默认高度。 */
        {
            ctu_cfg.altitude = 0U; /* 旧配置升级后的默认高度为0.00m，原有字段及偏移保持不变。 */
            config_upgraded = RT_TRUE; /* 统一在所有尾部字段补齐后只保存一次。 */
        }
        if(saved_payload_len < (offsetof(GSE8625_CfgTypeDef_Vlast, poll_interval_seconds) + sizeof(ctu_cfg.poll_interval_seconds) - sizeof(ctu_cfg.hdr))) /* 旧版配置不含轮询时间时补充10秒默认值。 */
        {
            ctu_cfg.poll_interval_seconds = 10U; /* 升级旧配置时使用用户确认的默认全局轮询时间。 */
            config_upgraded = RT_TRUE; /* 标记配置结构已升级，启动阶段需要写回新结构长度。 */
        }
        if(config_upgraded == RT_TRUE) /* 仅在确实补充过新增字段时保存，避免每次启动重复擦写Flash。 */
        {
            ctu_cfg_save(); /* 用当前结构长度重新保存A/B区，使新增字段在后续启动保持有效。 */
        }
    }
    /* Flash校验成功时保留已经保存的通信参数，禁止再次用默认值覆盖645写地址结果。 */
}

void set_default_para(void)
{
    // RJ45-2-2
    ctu_cfg.uart_protocol[UART1_NO]     = MODBUS_MASTER;//通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART1_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART1_NO]        = 1;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1
    // RS485-Ⅰ
    ctu_cfg.uart_protocol[UART3_NO]     = DLT645_SLAVE; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART3_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART3_NO]        = 3;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    // RJ45-2-1
    ctu_cfg.uart_protocol[UART4_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART4_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART4_NO]        = 1;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    // RJ45-1-2
    ctu_cfg.uart_protocol[UART5_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART5_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART5_NO]        = 1;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    // RS485-Ⅱ
    ctu_cfg.uart_protocol[UART6_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART6_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART6_NO]        = 1;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    // RJ45-1-1
    ctu_cfg.uart_protocol[UART7_NO]     = MODBUS_MASTER; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART7_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART7_NO]        = 1;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    // 载波
    ctu_cfg.uart_protocol[UART8_NO]     = DLT645_SLAVE; //通信协议 1=modbus协议,2=dlt645协议
    ctu_cfg.uart_baud[UART8_NO]         = 9600;         //波特率
    ctu_cfg.uart_check[UART8_NO]        = 3;            //校验位 1=8,N,1; 2=8,O,1 3=8,E,1

    for(uint8_t i=0; i<6; i++)
    {
        ctu_cfg.dlt645_bcd_addr[i]      = i;        //dlt645通信地址
    }

    ctu_cfg.longitude                   = 1143999;      //经度
    ctu_cfg.latitude                    = 304456;       //纬度
    ctu_cfg.g_vol_cal_coef              = 3562.04534;
    ctu_cfg.altitude                    = 0U;           /* 默认高度为0.00m，645写入位置信息后随配置统一保存。 */
    ctu_cfg.poll_interval_seconds       = 10U;          /* 全局逆变器周期抄读时间默认10秒。 */
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
