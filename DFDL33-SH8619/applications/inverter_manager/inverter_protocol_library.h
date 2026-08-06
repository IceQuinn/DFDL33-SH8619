/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#ifndef APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H
#define APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H

#include <stdint.h>

#include "inverter_archive.h"
#include "meas_cfg.h"
#include "AB_check.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 逆变器协议库固定提供100条厂家协议配置。 */
#define INVERTER_PROTOCOL_LIBRARY_COUNT             100U


/* 使用 0xFFFF 表示某个厂家不支持或未配置该寄存器。 */
#define INVERTER_PROTOCOL_REGISTER_UNUSED           UINT16_C(0xFFFF)

/* Modbus寄存器数据类型。
 * UINT16/INT16 占1个寄存器；UINT32/INT32/FLOAT32通常占2个寄存器；
 * UINT64/INT64/FLOAT64通常占4个寄存器。ASCII、BCD和BCD_TIME的实际长度
 * 由 reg_cnt 决定。 */
typedef enum Inv_RegDataType
{
    INVERTER_DATA_TYPE_UINT16 = 0,
    INVERTER_DATA_TYPE_INT16,
    INVERTER_DATA_TYPE_UINT32,
    INVERTER_DATA_TYPE_INT32,
    INVERTER_DATA_TYPE_UINT64,
    INVERTER_DATA_TYPE_INT64,
    INVERTER_DATA_TYPE_FLOAT32,
    INVERTER_DATA_TYPE_FLOAT64,
    INVERTER_DATA_TYPE_ASCII,
    INVERTER_DATA_TYPE_BCD,
    INVERTER_DATA_TYPE_BCD_TIME,
    INVERTER_DATA_TYPE_BIT_FIELD
} Inv_RegDataType_t;

/* 多字节数据排列方式。
 * 字母表示数据从最高有效字节到最低有效字节的正常顺序。例如32位数值的
 * 标准大端顺序为ABCD；CDAB表示两个16位寄存器交换；BADC表示每个寄存器
 * 内的两个字节交换；DCBA表示所有字节完全反序。 */
typedef enum Inv_ByteOrder
{
    INVERTER_BYTE_ORDER_AB = 0,
    INVERTER_BYTE_ORDER_BA,
    INVERTER_BYTE_ORDER_ABCD,
    INVERTER_BYTE_ORDER_CDAB,
    INVERTER_BYTE_ORDER_BADC,
    INVERTER_BYTE_ORDER_DCBA
} Inv_ByteOrder_t;

/* 单个Modbus寄存器块的通用描述。 此结构体不保存实时数据，只描述一个测量点的地址、长度和数据解析方式。 读写功能码不在寄存器块中保存，由三大分类对应的业务逻辑或厂家协议驱动 统一确定。 */
#pragma pack(1)
typedef struct Inv_RegBlk
{
    /* Modbus PDU中的零基寄存器地址。例如设备手册中的保持寄存器40001，
     * 如果手册采用1基编号，转换后的PDU地址通常是0。协议配置必须统一使用
     * PDU地址，禁止同时混用40001编号和零基地址。 */
    uint16_t reg_addr;

    /* 读取的16位Modbus寄存器数量，不是字节数量。 */
    uint16_t reg_cnt;

    /* 数据类型，取值见 Inv_RegDataType_t。 */
    uint8_t data_type;

    /* 字节及字序，取值见 Inv_ByteOrder_t。 */
    uint8_t byte_order;

    /* 定点数的小数位数。实际值 = 解析后的整数值 / 10^decimal_places。
     * 例如寄存器原始值2301、decimal_places为1，实际值为230.1。
     * 浮点、ASCII、时间和位域类型通常配置为0。 */
    uint8_t decimal_places;
} Inv_RegBlk_t;


/* 数据类：运行过程中周期采集的只读数据。*/
typedef struct Inv_ProtoData
{
    /* A、B、C三相电压寄存器，每相独立配置起始地址和读取数量，并分别发起 Modbus读取。数组下标使用 INVERTER_PROTOCOL_PHASE_A/B/C。 */
    Inv_RegBlk_t volt[ENUM_PHASE_MAX];

    /* A、B、C三相电流寄存器，每相独立配置起始地址和读取数量，并分别发起 Modbus读取。数组下标使用 INVERTER_PROTOCOL_PHASE_A/B/C。 */
    Inv_RegBlk_t curr[ENUM_PHASE_MAX];

    /* A、B、C三相有功功率寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t active_pwr[ENUM_PHASE_MAX];

    /* A、B、C三相无功功率寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t reactive_pwr[ENUM_PHASE_MAX];

    /* A、B、C三相功率因数寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t pwr_factor[ENUM_PHASE_MAX];

} Inv_ProtoData_t;


/* 当前字段布局在1字节对齐下应固定占用105字节：
 * 三相电压、三相电流               3×7×2 = 42字节
 * 三相有功、三相无功、三相功率因数 3×7×3 = 63字节
 * 合计105字节
 * 使用C99兼容的负数组长度方式执行编译期检查；后续增删字段却没有同步更新
 * 期望大小时，编译器会直接报错。 */
#define INV_PROTO_DATA_SIZE                         105U
typedef char Inv_ProtoDataSizeCheck_t[
    (sizeof(Inv_ProtoData_t) == INV_PROTO_DATA_SIZE) ? 1 : -1];

/* 参数类：逆变器的只读基础参数。 本需求将参数类定义为只读。如果某厂家允许修改设定电压，应另外在控制类 中增加对应写入点，不能直接改变参数类的访问语义。 */
typedef struct Inv_ProtoParam
{
    /* 设备编号或序列号，可能是整数、BCD或ASCII字符串。 */
    Inv_RegBlk_t dev_no;

    /* PV额定有功功率。 */
    Inv_RegBlk_t pv_rated_active_pwr;

    /* PV额定无功功率。 */
    Inv_RegBlk_t pv_rated_reactive_pwr;

    /* 逆变器设定电压。 */
    Inv_RegBlk_t set_volt;

    /* 输出类型，例如单相、三相三线或三相四线，通常为枚举或位域。 */
    Inv_RegBlk_t output_type;

    /* 开关机状态 */
    Inv_RegBlk_t pwr_status;
} Inv_ProtoParam_t;

/* 参数类包含6个寄存器块，按1字节对齐后共7×6=42字节。 */
#define INV_PROTO_PARAM_SIZE                        42U
typedef char Inv_ProtoParamSizeCheck_t[
    (sizeof(Inv_ProtoParam_t) == INV_PROTO_PARAM_SIZE) ? 1 : -1];

/* 一个固定命令值控制点。适用于开机、关机、复位等“写入固定值触发”的控制。 即使开机和关机共用同一寄存器，也分别配置两个控制点，以兼容某些厂家将 开机和关机定义在不同地址的情况。 */
typedef struct Inv_FixedCmd
{
    /* 控制寄存器的地址、长度、数据类型、字节顺序和小数位。 */
    Inv_RegBlk_t reg;

    /* 执行该控制时写入寄存器的原始值。 */
    uint32_t cmd_val;
} Inv_FixedCmd_t;

#define INV_FIXED_CMD_SIZE                          11U
typedef char Inv_FixedCmdSizeCheck_t[
    (sizeof(Inv_FixedCmd_t) == INV_FIXED_CMD_SIZE) ? 1 : -1];

/* 控制类：允许读取当前值并写入控制命令的寄存器。
 * 开机和关机可能共用同一个地址但写入值不同，也可能使用不同寄存器，因此
 * 使用两个独立的固定命令控制点。控制百分比及功率值按各自
 * decimal_places 转换。 */
typedef struct Inv_ProtoCtrl
{
    /* 逆变器开机控制寄存器。 */
    Inv_RegBlk_t pwr_on;

    /* 逆变器关机控制寄存器。 */
    Inv_RegBlk_t pwr_off;

    /* 有功功率数值控制寄存器。 */
    Inv_RegBlk_t active_pwr_ctrl;

    /* 无功功率数值控制寄存器。 */
    Inv_RegBlk_t reactive_pwr_ctrl;
    
    /* 功率因数控制寄存器。 */
    Inv_RegBlk_t pwr_factor_ctrl;

    /* 有功功率百分比控制寄存器。 */
    Inv_RegBlk_t active_pwr_pct_ctrl;

    /* 无功功率百分比控制寄存器。 */
    Inv_RegBlk_t reactive_pwr_pct_ctrl;
} Inv_ProtoCtrl_t;

/* 控制类包含7个控制寄存器块，按1字节对齐后共7×7=49字节。 */
#define INV_PROTO_CTRL_SIZE                         49U
typedef char Inv_ProtoCtrlSizeCheck_t[
    (sizeof(Inv_ProtoCtrl_t) == INV_PROTO_CTRL_SIZE) ? 1 : -1];

/* 一条完整逆变器协议配置，包含数据类、参数类和控制类。 */
typedef struct Inv_Proto
{
    rcd_head head;
    /* 与逆变器档案共用的厂家名称及规约版本。 */
    Inv_MfrInfo_t mfr_info;

    /* 周期采集的只读运行数据。 */
    Inv_ProtoData_t data;

    /* 只读基础参数。 */
    Inv_ProtoParam_t param;

    /* 控制类。 */
    Inv_ProtoCtrl_t ctrl;
} Inv_Proto_t;
#pragma pack()

/* 厂家信息34字节、数据类105字节、参数类42字节、控制类49字节，共230字节。 */
#define INV_PROTO_SIZE                              236U
typedef char Inv_ProtoSizeCheck_t[
    (sizeof(Inv_Proto_t) == INV_PROTO_SIZE) ? 1 : -1];

/* 全局逆变器协议库，数组下标范围为0~99，前4项分别对应阳光、华为、固德威和锦浪。 */
extern Inv_Proto_t g_inv_proto_lib[INVERTER_PROTOCOL_LIBRARY_COUNT];


void Inv_Proto_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H */
