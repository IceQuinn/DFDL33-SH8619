/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-18     IP155       the first version
 */
#ifndef APPLICATIONS_SYS_H_
#define APPLICATIONS_SYS_H_

#include <stdint.h>

#ifndef countof
#define countof(x)      (sizeof(x)/sizeof(x[0]))
#endif

#ifndef PRINT_VAR_NAME
#define PRINT_VAR_NAME(name)    (#name)
#endif

#define SQUARE(x)   (x)*(x)

#define DO_NOT_SAVE_TIME    60*8
#define POWER_DOWN_SAVE_WIN 30
extern uint32_t Power_UP_Time;
extern uint32_t Power_Down_Time;



//数据类型
enum    data_type
{
    TYPE_NONE,
    TYPE_I8,
    TYPE_U8,
    TYPE_I16,
    TYPE_U16,
    TYPE_I32,
    TYPE_U32,
    TYPE_FLOAT,
};

//读写权限
enum
{
    _R,
    _W,
    _RW,
};

enum{
    POSITIVE_NUM,
    NEGATIVE_NUM,
};

typedef struct
{
    uint16_t    major_version_number;
    uint16_t    minor_version_number;
    uint16_t    revision_number;
    uint16_t    build_number;
}APP_Version;
extern const APP_Version g_app_version;
extern const APP_Version g_show_app_ver;

typedef struct project_date
{
    uint16_t hardware_year;     //工程编译年份
    uint16_t hardware_mon;      //工程编译月份
    uint16_t hardware_day;      //工程编译日份
}project_date;
extern project_date hardware_date;

typedef struct
{
    uint16_t    major_version_number;
    uint16_t    minor_version_number;
}ModBus_Version;
extern const ModBus_Version g_modbus_version;


typedef struct
{
    uint16_t ver;       // 版本号
    uint16_t len;       // 配置长度，从ugain_a开始到结构体结束
    uint16_t crc16b;    // 校验和（从第一个参数开始计算，len长度）
}rcd_header;


extern char app_firmware_ver_ascll[];
extern char app_show_ver_ascll[];

void show_ctu_msg(void);



int32_t SetDataFromAddr(uint8_t DataType, const void *PAddr, int32_t val);
/* 从指定地址读数据 */
int32_t GetDataFromAddr(uint8_t DataType, const uint32_t *PAddr);
/* 从根据类型返回该类型字节数 */
int8_t  GetDataByteFromType(uint8_t DataType);

void show_arr(char* name, void* data, int len);

void Sys_Run_Time_Init(void);
void Sys_Power_Down_CB(void);
int32_t is_power_down(void);
int32_t is_power_down_legal_work(void);

#endif /* APPLICATIONS_SYS_H_ */
