/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-17     IP155       the first version
 */
#ifndef APPLICATIONS_CTU_CFG_CTU_CFG_H_
#define APPLICATIONS_CTU_CFG_CTU_CFG_H_

#include "sys.h"

#include <stdint.h>
#include "AB_check.h"
#include "uart_def.h"

enum {
    MODBUS_SLAVE = 1,   // Modbus 从机协议
    DLT645_SLAVE    ,   // DLT645 从机协议
    MODBUS_MASTER   ,   // MOdbus 主机协议
};

/* --------------------------------------------------------------------------------------------配置参数 */
#define CTU_CFG_VER        1       //配置参数版本号

#pragma pack(1) //一字节对齐
/* Ver:1 */
typedef struct GSE8615_CONFIG_V1
{
    rcd_head hdr;
    /* 通信参数 */
    uint16_t uart_protocol[UART_NO_MAXS];           //通信协议 1=modbus协议,2=dlt645协议
    uint32_t uart_baud[UART_NO_MAXS];               //串口波特率
    uint16_t uart_check[UART_NO_MAXS];              //串口校验位格式

    uint8_t dlt645_bcd_addr[6];                     //dlt645通信地址

    uint32_t longitude;                             //经度
    uint32_t latitude;                              //纬度

}GSE8625_CfgTypeDef_Vlast;

#pragma pack()
// 本地全局变量
extern GSE8625_CfgTypeDef_Vlast ctu_cfg;


void Ctu_Cfg_Init(void);
void set_default_para(void);        //设置默认值
void set_default_data(void);
void ctu_cfg_save(void);

int baud_check(uint32_t baud);
uint32_t phm_baud_to_val(uint32_t baud);
uint32_t phm_val_to_buad(uint32_t val);

uint16_t read_password(uint8_t password_addr);//密码



uint8_t check_password(uint8_t password_addr);
uint16_t change_show(uint16_t addr);


#endif /* APPLICATIONS_CTU_CFG_CTU_CFG_H_ */

