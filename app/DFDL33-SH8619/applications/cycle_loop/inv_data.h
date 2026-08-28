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
#include <rtthread.h>

#include "inverter_archive.h"
#include "meas_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 设备编号在线上最多占用32字节，内存中额外保留一个字符串结束符。 */
#define INV_DATA_DEVICE_NO_MAX_LEN                  32U

/* 逆变器控制类型与协议库Inv_ProtoCtrl_t中的控制寄存器一一对应。 */
typedef enum Inv_Control_Type
{
    INV_CONTROL_POWER_ON = 0,             /* 使用协议默认值执行开机控制。 */
    INV_CONTROL_POWER_OFF,                /* 使用协议默认值执行关机控制。 */
    INV_CONTROL_ACTIVE_POWER,             /* 写入有功功率数值。 */
    INV_CONTROL_REACTIVE_POWER,           /* 写入无功功率数值。 */
    INV_CONTROL_POWER_FACTOR,             /* 写入功率因数。 */
    INV_CONTROL_ACTIVE_POWER_PERCENT,     /* 写入有功功率百分比。 */
    INV_CONTROL_REACTIVE_POWER_PERCENT,   /* 写入无功功率百分比。 */
    INV_CONTROL_TYPE_MAX                  /* 控制类型数量，仅用于参数范围检查。 */
} Inv_Control_Type_t;

/* 已受理控制请求的最终执行结果，提交接口返回成功不代表控制已经完成。 */
typedef enum Inv_Control_Result
{
    INV_CONTROL_RESULT_OK = 0,            /* Modbus写响应完整校验通过。 */
    INV_CONTROL_RESULT_ARCHIVE_INVALID,   /* 目标档案无效或已经失效。 */
    INV_CONTROL_RESULT_PROTOCOL_MISSING,  /* 目标档案没有匹配的协议对象。 */
    INV_CONTROL_RESULT_UNSUPPORTED,       /* 协议未配置该控制项或配置不支持。 */
    INV_CONTROL_RESULT_BUILD_FAILED,      /* 控制值转换或Modbus写请求组帧失败。 */
    INV_CONTROL_RESULT_SEND_FAILED,       /* 写请求没有完整写入下行串口。 */
    INV_CONTROL_RESULT_TIMEOUT,           /* 写请求发送后1秒内没有收到响应。 */
    INV_CONTROL_RESULT_RESPONSE_INVALID,  /* 收到响应，但CRC、地址、功能码或长度错误。 */
    INV_CONTROL_RESULT_DEVICE_EXCEPTION   /* 逆变器返回Modbus异常响应。 */
} Inv_Control_Result_t;

/* 调用方提交的异步控制请求，request_id由调用方生成并在结果中原样返回。 */
typedef struct Inv_Control_Request
{
    uint32_t request_id;                   /* 调用方用于关联请求和结果的流水号。 */
    int32_t value;                         /* 协议定点整数值，开关机控制时忽略该字段。 */
    uint8_t archive_index;                 /* 目标档案槽位下标，范围0～11。 */
    Inv_Control_Type_t type;               /* 本次需要执行的逆变器控制类型。 */
} Inv_Control_Request_t;

/* 控制完成结果由查询接口取出，Modbus异常码只在DEVICE_EXCEPTION时有效。 */
typedef struct Inv_Control_Result_Info
{
    Inv_Control_Request_t request;         /* 已完成的控制请求，开关机会带回实际默认写入值。 */
    Inv_Control_Result_t result;           /* 本次控制最终执行结果。 */
    uint8_t exception_code;                /* 逆变器返回的Modbus异常码，其他结果固定为0。 */
    uint32_t finish_tick;                  /* 生成控制结果时rt_tick_get()返回的实时tick。 */
} Inv_Control_Result_Info_t;

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

    /* value中不包含字符串结束符的有效字节数量。 */
    uint8_t length;

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
} Inv_RealtimeParam_t;

/* 控制类实时数据，用于记录最近一次成功执行或确认的控制值。 */
typedef struct Inv_RealtimeCtrl
{
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
    Inv_RealtimeValue_t daily_energy; /* 日发电量实时数据，与协议结构末尾寄存器对应。 */
} Inv_Data_t;

/* 实时数据只保存在RAM中，下标与g_inv_archive_lib中的档案槽位一一对应。 */
extern Inv_Data_t g_inv_data[INVERTER_ARCHIVE_MAX_COUNT];

/* 初始化三个端口的独立周期抄读状态机，并清空全部档案实时数据。 */
void Inv_Data_Init(void);

/* 周期抄读主循环，三个有效端口的发送、接收和超时状态分别推进。 */
void Inv_Data_Poll_Loop(void);

/* 串口管理层调用本接口提交周期抄读阶段收到的一帧完整响应。 */
rt_err_t Inv_Data_Rx_Frame(uint16_t uart_no, const uint8_t *frame, uint16_t frame_len);

/* 异步提交逆变器控制请求，返回RT_EOK仅表示请求已经进入目标端口队列。 */
rt_err_t Inv_Control_Submit(const Inv_Control_Request_t *request);

/* 等待并取出一项控制结果，timeout单位为系统tick，支持0和RT_WAITING_FOREVER。 */
rt_err_t Inv_Control_Get_Result(Inv_Control_Result_Info_t *result, int32_t timeout);

/* 按0～11档案槽位获取实时数据，槽位无效或越界时返回RT_NULL。 */
Inv_Data_t *Inv_Data_Get(uint8_t archive_index);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_INV_DATA_H_ */
