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
#define CTU_CFG_VER        2       //配置参数版本号

#pragma pack(1) //一字节对齐
/* Ver:1 */
typedef struct GSE8615_CONFIG_V1
{
    rcd_head hdr;
    /* 通信参数 */
    uint16_t uart_protocol[UART_NO_MAXS];           //通信协议 1=modbus协议,2=dlt645协议
    uint32_t uart_baud[UART_NO_MAXS];               //串口波特率
    uint16_t uart_check[UART_NO_MAXS];              //串口校验位格式 1=8,N,1; 2=8,O,1 3=8,E,1

    uint8_t dlt645_bcd_addr[6];                     //dlt645通信地址

    uint32_t longitude;                             //经度
    uint32_t latitude;                              //纬度
    float g_vol_cal_coef;                           //单向采集电压校准系数
    uint32_t altitude;                              /* 高度，单位0.01m；追加在结构体尾部以兼容旧版Flash配置字段偏移。 */
    uint16_t poll_interval_seconds;                 /* 全局逆变器周期抄读间隔，单位秒，0关闭周期抄读，其余有效范围5～3600。 */

    uint16_t overvoltage_setting;                   /* 电压越限事件触发限值，单位0.1V，对应04E63002第一字段。 */
    uint32_t overvoltage_t_delay;                   /* 电压越限判定持续时间，单位秒，对应04E63002第二字段。 */

    /* 04E63001光伏逆变器通信异常事件触发限值，四项均为全局共享配置。 */
    uint32_t comm_err_start_seconds;                /* 监测起始时间，单位秒，一天内的秒数0～86399，对应hhmmss。 */
    uint32_t comm_err_end_seconds;                  /* 监测结束时间，单位秒，一天内的秒数0～86399，对应hhmmss。 */
    uint16_t comm_err_interval_minutes;             /* 监测间隔，单位分钟，0关闭异常统计，1～9999有效（NNNN两字节BCD），对应NNNN。 */
    uint8_t  comm_err_daily_max_count;              /* 日发生最大次数，0表示不限制，1～99有效（NN为一字节BCD），对应NN。 */

}GSE8625_CfgTypeDef_Vlast;

#pragma pack()
// 本地全局变量
extern GSE8625_CfgTypeDef_Vlast ctu_cfg;


void Ctu_Cfg_Init(void);
void set_default_data(void);
void ctu_cfg_save(void);

int baud_check(uint32_t baud);
uint32_t phm_baud_to_val(uint32_t baud);
uint32_t phm_val_to_buad(uint32_t val);

uint16_t read_password(uint8_t password_addr);//密码



uint8_t check_password(uint8_t password_addr);
uint16_t change_show(uint16_t addr);


#endif /* APPLICATIONS_CTU_CFG_CTU_CFG_H_ */

