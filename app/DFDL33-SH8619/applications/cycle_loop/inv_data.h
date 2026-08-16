/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-16     mutou       the first version
 */
#ifndef APPLICATIONS_CYCLE_LOOP_INV_DATA_H_
#define APPLICATIONS_CYCLE_LOOP_INV_DATA_H_

#include <stdint.h>

#include "meas_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 设备编号在线上最多占用32字节，内存中额外保留一个字符串结束符。 */
#define INV_DATA_DEVICE_NO_MAX_LEN                  32U

/* 单个数值型实时数据项，协议原始值完成数据类型和字节序转换后写入value。 */
typedef struct Inv_RealtimeValue
{
    /* 统一使用int32_t保存实时值，小数按照协议配置的decimal_places保留为定点整数。 */
    int32_t value;

    /* 最近一次成功更新时rt_tick_get()返回的tick，当前工程中1tick等于1ms。 */
    uint32_t update_tick;

    /* 1表示value和update_tick有效，0表示尚未读取成功或本协议未配置该数据项。 */
    uint8_t valid;
} Inv_RealtimeValue_t;

/* 设备编号实时数据项，单独保存字符串以兼容协议库中的ASCII或BCD设备编号。 */
typedef struct Inv_RealtimeString
{
    /* 最后一个字节始终预留给'\0'，因此最多保存32字节有效设备编号。 */
    char value[INV_DATA_DEVICE_NO_MAX_LEN + 1U];

    /* 最近一次成功更新时rt_tick_get()返回的tick。 */
    uint32_t update_tick;

    /* 1表示value和update_tick有效，0表示尚未读取成功或本协议未配置设备编号。 */
    uint8_t valid;
} Inv_RealtimeString_t;

/* 数据类实时数据，字段和Inv_ProtoData_t保持一一对应。 */
typedef struct Inv_RealtimeData
{
    Inv_RealtimeValue_t Ux[ENUM_PHASE_MAX]; /* A、B、C三相电压。 */
    Inv_RealtimeValue_t Ix[ENUM_PHASE_MAX]; /* A、B、C三相电流。 */
    Inv_RealtimeValue_t Px[ENUM_PMAX];      /* A、B、C三相及总有功功率。 */
    Inv_RealtimeValue_t Qx[ENUM_QMAX];      /* A、B、C三相及总无功功率。 */
    Inv_RealtimeValue_t PFx[ENUM_PFMAX];    /* A、B、C三相及总功率因数。 */
} Inv_RealtimeData_t;

/* 参数类实时数据，字段和Inv_ProtoParam_t保持一一对应。 */
typedef struct Inv_RealtimeParam
{
    Inv_RealtimeString_t dev_no;                  /* 设备编号或序列号。 */
    Inv_RealtimeValue_t pv_rated_active_pwr;      /* PV额定有功功率。 */
    Inv_RealtimeValue_t pv_rated_reactive_pwr;    /* PV额定无功功率。 */
    Inv_RealtimeValue_t set_volt;                 /* 逆变器设定电压。 */
    Inv_RealtimeValue_t output_type;              /* 逆变器输出类型。 */
    Inv_RealtimeValue_t pwr_status;               /* 逆变器开关机状态。 */
} Inv_RealtimeParam_t;

/* 控制类实时数据，用于记录最近一次成功执行或确认的控制值。 */
typedef struct Inv_RealtimeCtrl
{
    Inv_RealtimeValue_t pwr_on;                   /* 最近一次开机控制值。 */
    Inv_RealtimeValue_t pwr_off;                  /* 最近一次关机控制值。 */
    Inv_RealtimeValue_t active_pwr_ctrl;          /* 有功功率控制值。 */
    Inv_RealtimeValue_t reactive_pwr_ctrl;        /* 无功功率控制值。 */
    Inv_RealtimeValue_t pwr_factor_ctrl;          /* 功率因数控制值。 */
    Inv_RealtimeValue_t active_pwr_pct_ctrl;      /* 有功功率百分比控制值。 */
    Inv_RealtimeValue_t reactive_pwr_pct_ctrl;    /* 无功功率百分比控制值。 */
} Inv_RealtimeCtrl_t;

/* 单台逆变器的完整实时数据，档案槽位可直接对应一个Inv_Data_t对象。 */
typedef struct Inv_Data
{
    Inv_RealtimeData_t data;    /* 周期抄读的数据类实时数据。 */
    Inv_RealtimeParam_t param;  /* 周期或初始化抄读的参数类实时数据。 */
    Inv_RealtimeCtrl_t ctrl;    /* 最近一次控制类实时数据。 */
} Inv_Data_t;

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_INV_DATA_H_ */
