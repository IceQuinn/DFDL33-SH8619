/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-18     IP155       the first version
 */
#include <stdio.h>
#include <time.h>

#include "drv_common.h"

#define DBG_TAG "sys"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "sys.h"
#include "user_comm.h"
#include "user_rtc.h"
//#include "user_gpio.h"

#define OS_YEAR     ((((__DATE__ [7] - '0') * 10 + (__DATE__ [8] - '0')) * 10 + (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

#define OS_MONTH    (__DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? 1 : 6) \
                                : __DATE__ [2] == 'b' ? 2 \
                                : __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? 3 : 4) \
                                : __DATE__ [2] == 'y' ? 5 \
                                : __DATE__ [2] == 'l' ? 7 \
                                : __DATE__ [2] == 'g' ? 8 \
                                : __DATE__ [2] == 'p' ? 9 \
                                : __DATE__ [2] == 't' ? 10 \
                                : __DATE__ [2] == 'v' ? 11 : 12)

#define OS_DAY      ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 + (__DATE__ [5] - '0'))

#define OS_HOUR     ((__TIME__ [0] - '0') * 10 + (__TIME__ [1] - '0'))

#define OS_MINUTE   ((__TIME__ [3] - '0') * 10 + (__TIME__ [4] - '0'))

#define OS_SECOND   ((__TIME__ [6] - '0') * 10 + (__TIME__ [7] - '0'))

#define PROJECT_NAME    "DFDL33-SH8619"

const APP_Version g_app_version  = {1, 0, 0, 28};       // 研发内部固件版本（对内版本）
const APP_Version g_show_app_ver = {1, 0, 0, 1};        // 研发发布固件版本（对外版本）
char PHM_Ver[32] = "PHM V1.0.1.0 2025.07.01";           // 电鸿版本号
char app_firmware_ver_ascll[32]  = {0};                 //固件版本号以及更新日期
char app_show_ver_ascll[32] = {0};                      //软件版本号已经更新日期

const ModBus_Version g_modbus_version = {1, 0};         //ModBus版本

project_date hardware_date;

void printf_project_name(void)
{
    LOG_D("DEVICE NAME:   %s", PROJECT_NAME);
}

void check_compile_time(void)
{
    hardware_date.hardware_year = OS_YEAR;
    hardware_date.hardware_mon  = OS_MONTH;
    hardware_date.hardware_day  = OS_DAY;

    LOG_D("COMPILED DATE: %s at %s", __DATE__, __TIME__);
}

void show_ctu_msg(void)
{
     /* 打印装置名称 */
    printf_project_name();

    /* 打印编译时间 */
    check_compile_time();

    /* 研发内部固件版本（对内版本） */
    rt_sprintf(app_firmware_ver_ascll, "%d.%d.%d.%d %02d-%02d-%02d",
                g_app_version.major_version_number,
                g_app_version.minor_version_number,
                g_app_version.revision_number,
                g_app_version.build_number,
                hardware_date.hardware_year-2000,
                hardware_date.hardware_mon,
                hardware_date.hardware_day);
    LOG_D("FIRMWARE VER:  %s", app_firmware_ver_ascll);

    /* 研发发布固件版本（对外版本） */
    rt_sprintf(app_show_ver_ascll, "%d.%d.%d.%d %02d-%02d-%02d",
            g_show_app_ver.major_version_number,
            g_show_app_ver.minor_version_number,
            g_show_app_ver.revision_number,
            g_show_app_ver.build_number,
            25, 7, 1);
    LOG_D("SOFT VER:      %s\n", app_show_ver_ascll);
}
MSH_CMD_EXPORT(show_ctu_msg, show_ctu_msg);

int32_t SetDataFromAddr(uint8_t DataType, const void *PAddr, int32_t val)
{
    switch(DataType)
    {
    case TYPE_I8:
        *((int8_t *)PAddr) = val;
        break;
    case TYPE_U8:
        *((uint8_t *)PAddr) = val;
        break;
    case TYPE_I16:
        *((int16_t *)PAddr) = val;
        break;
    case TYPE_U16:
        *((uint16_t *)PAddr) = val;
        break;
    case TYPE_I32:
        *((int32_t *)PAddr) = val;
        break;
    case TYPE_U32:
        *((uint32_t *)PAddr) = val;
        break;
    case TYPE_FLOAT32:
        *((float *)PAddr) = *((float *)&val);
        break;
    default:
        LOG_E("Error DataType");
        break;
    }
    return RT_EOK;
}

int32_t GetDataFromAddr(uint8_t DataType, const uint32_t *PAddr)
{
    int32_t Val = 0;
    switch(DataType)
    {
    case TYPE_I8:
        Val = (int32_t )*((int8_t *)PAddr);
        break;
    case TYPE_U8:
        Val = (int32_t )*((uint8_t *)PAddr);
        break;
    case TYPE_I16:
        Val = (int32_t )*((int16_t *)PAddr);
        break;
    case TYPE_U16:
        Val = (int32_t )*((uint16_t *)PAddr);
        break;
    case TYPE_I32:
        Val = (int32_t )*((int32_t *)PAddr);
        break;
    case TYPE_U32:
        Val = (int32_t )*((uint32_t *)PAddr);
        break;
    case TYPE_FLOAT32:
        Val = (int32_t )*((float *)PAddr);
        break;
    default:
        LOG_E("Error DataType");
        break;
    }
    return Val;
}

int8_t GetDataByteFromType(uint8_t DataType)
{
    int8_t data_byte = -1;
    switch(DataType)
    {
    case TYPE_I8:
    case TYPE_U8:
        data_byte = 1;
        break;
    case TYPE_I16:
    case TYPE_U16:
        data_byte = 2;
        break;
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_FLOAT32:
        data_byte = 4;
        break;
    default :
        LOG_E("Error DataType");
        break;
    }
    return data_byte;
}

/* 按十六进制逐字节打印数组内容，整行日志只读取一次RTC时间。 */
void show_arr(const char *name, const void *data, uint32_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t index;

    rt_kprintf("%s %s[%03d] =", get_char_time(), name, len);

    /* 每个数组元素使用%02x输出。 */
    for(index = 0U; index < len; ++index) {
        rt_kprintf(" %02x", ptr[index]);
    }

    rt_kprintf("\n");
}


uint32_t Power_UP_Time = 0;
uint32_t Power_Down_Time = 0;

// 记录上电时间
void Sys_Run_Time_Init(void)
{
    if(0 == Power_UP_Time)
    {
        Power_UP_Time = time(NULL);
    }
}

// 记录掉电时间
void Sys_Power_Down_CB(void)
{
    if(0 == Power_Down_Time)
    {
        Power_Down_Time = time(NULL);
    }
}

// 是否掉电
int32_t is_power_down(void)
{
    if(Power_Down_Time){
        return 1;
    }
    else {
        return 0;
    }
}

// 询问掉电后还能否继续工作
int32_t is_power_down_legal_work(void)
{
    // 超过8分钟，并且在8分30s内，功能可正常运行
    if(((Power_Down_Time - Power_UP_Time) > DO_NOT_SAVE_TIME) && ((time(NULL) - Power_Down_Time) < POWER_DOWN_SAVE_WIN))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}




