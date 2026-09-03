/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-06     mutou       the first version
 */
#include "inverter_protocol_library.h"
#include <rtthread.h>

typedef enum Enum_Inv_Mfr_Id
{
    INV_MFR_SUNGROW_1 = 0,
    INV_MFR_HUAWEI_1,
    INV_MFR_HUAWEI_2,
    INV_MFR_GOODWE_1,
    INV_MFR_GOODWE_2,
    INV_MFR_GROWATT_1,
    INV_MFR_GROWATT_2,
    INV_MFR_SAJ_1,
    INV_MFR_SAJ_2,
    INV_MFR_SAJ_3,
    INV_MFR_JINLONG_1,
    INV_MFR_SOFAR_1,
    INV_MFR_SINENG_1,
    INV_MFR_AUXSOL_1,
    INV_MFR_AUXSOL_2,
    INV_MFR_MAX
}Enum_Inv_Mfr_Id_t;
// ----------------------------------------------------厂家信息
typedef struct Inv_Mfr_Id{
    Enum_Inv_Mfr_Id_t mfr_id; /* 用于把厂家信息装配到同编号默认协议。 */
    Inv_MfrInfo_t mfr_info;   /* 厂家ASCII名称和规约版本。 */
}Inv_Mfr_Id_t;
// ----------------------------------------------------厂家特征数据
typedef struct Inv_Feature_Id{
    Enum_Inv_Mfr_Id_t mfr_id; /* 用于把特征配置装配到同编号默认协议。 */
    Inv_Feature_t feature;     /* 自动识别使用的特征寄存器配置和有效范围。 */
}Inv_Feature_Id_t;
// ----------------------------------------------------厂家数据类
typedef struct Inv_ProtoData_Ua_Id{ // A相电压
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前A相电压配置所属厂家编号。 */
    Inv_RegBlk_t Ua;           /* A相电压只读寄存器配置。 */
}Inv_ProtoData_Ua_Id_t;
typedef struct Inv_ProtoData_Ub_Id{ // B相电压
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前B相电压配置所属厂家编号。 */
    Inv_RegBlk_t Ub;           /* B相电压只读寄存器配置。 */
}Inv_ProtoData_Ub_Id_t;
typedef struct Inv_ProtoData_Uc_Id{ // C相电压
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前C相电压配置所属厂家编号。 */
    Inv_RegBlk_t Uc;           /* C相电压只读寄存器配置。 */
}Inv_ProtoData_Uc_Id_t;
typedef struct Inv_ProtoData_Ia_Id{ // A相电流
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前A相电流配置所属厂家编号。 */
    Inv_RegBlk_t Ia;           /* A相电流只读寄存器配置。 */
}Inv_ProtoData_Ia_Id_t;
typedef struct Inv_ProtoData_Ib_Id{ // B相电流
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前B相电流配置所属厂家编号。 */
    Inv_RegBlk_t Ib;           /* B相电流只读寄存器配置。 */
}Inv_ProtoData_Ib_Id_t;
typedef struct Inv_ProtoData_Ic_Id{ // C相电流
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前C相电流配置所属厂家编号。 */
    Inv_RegBlk_t Ic;           /* C相电流只读寄存器配置。 */
}Inv_ProtoData_Ic_Id_t;
typedef struct Inv_ProtoData_Pa_Id{ // A相有功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前A相有功功率配置所属厂家编号。 */
    Inv_RegBlk_t Pa;           /* A相有功功率只读寄存器配置。 */
}Inv_ProtoData_Pa_Id_t;
typedef struct Inv_ProtoData_Pb_Id{ // B相有功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前B相有功功率配置所属厂家编号。 */
    Inv_RegBlk_t Pb;           /* B相有功功率只读寄存器配置。 */
}Inv_ProtoData_Pb_Id_t;
typedef struct Inv_ProtoData_Pc_Id{ // C相有功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前C相有功功率配置所属厂家编号。 */
    Inv_RegBlk_t Pc;           /* C相有功功率只读寄存器配置。 */
}Inv_ProtoData_Pc_Id_t;
typedef struct Inv_ProtoData_Pt_Id{ // 总有功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前总有功功率配置所属厂家编号。 */
    Inv_RegBlk_t Pt;           /* 总有功功率只读寄存器配置。 */
}Inv_ProtoData_Pt_Id_t;

typedef struct Inv_ProtoData_Qa_Id{ // A相无功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前A相无功功率配置所属厂家编号。 */
    Inv_RegBlk_t Qa;           /* A相无功功率只读寄存器配置。 */
}Inv_ProtoData_Qa_Id_t;
typedef struct Inv_ProtoData_Qb_Id{ // B相无功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前B相无功功率配置所属厂家编号。 */
    Inv_RegBlk_t Qb;           /* B相无功功率只读寄存器配置。 */
}Inv_ProtoData_Qb_Id_t;
typedef struct Inv_ProtoData_Qc_Id{ // C相无功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前C相无功功率配置所属厂家编号。 */
    Inv_RegBlk_t Qc;           /* C相无功功率只读寄存器配置。 */
}Inv_ProtoData_Qc_Id_t;
typedef struct Inv_ProtoData_Qt_Id{ // 总无功功率
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前总无功功率配置所属厂家编号。 */
    Inv_RegBlk_t Qt;           /* 总无功功率只读寄存器配置。 */
}Inv_ProtoData_Qt_Id_t;
typedef struct Inv_ProtoData_PFa_Id{ // A相功率因数
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前A相功率因数配置所属厂家编号。 */
    Inv_RegBlk_t PFA;          /* A相功率因数只读寄存器配置。 */
}Inv_ProtoData_PFa_Id_t;
typedef struct Inv_ProtoData_PFb_Id{ // B相功率因数
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前B相功率因数配置所属厂家编号。 */
    Inv_RegBlk_t PFb;          /* B相功率因数只读寄存器配置。 */
}Inv_ProtoData_PFb_Id_t;
typedef struct Inv_ProtoData_PFc_Id{ // C相功率因数
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前C相功率因数配置所属厂家编号。 */
    Inv_RegBlk_t PFc;          /* C相功率因数只读寄存器配置。 */
}Inv_ProtoData_PFc_Id_t;
typedef struct Inv_ProtoData_PFt_Id{ // 总功率因数
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前总功率因数配置所属厂家编号。 */
    Inv_RegBlk_t PFt;          /* 总功率因数只读寄存器配置。 */
}Inv_ProtoData_PFt_Id_t;
// ----------------------------------------------------厂家参数
typedef struct Inv_dev_no_Id{   // 设备编号或序列号
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前设备编号配置所属厂家编号。 */
    Inv_RegBlk_t dev_no;       /* 设备编号或序列号只读寄存器配置。 */
}Inv_dev_no_Id_t;
typedef struct Inv_Pn_Id{ /* PV额定有功功率协议配置项。 */
    Enum_Inv_Mfr_Id_t mfr_id;         /* 当前额定有功功率配置所属厂家编号。 */
    Inv_RegBlk_t Pn;                   /* PV额定有功功率只读寄存器配置。 */
} Inv_Pn_Id_t;
typedef struct Inv_Qn_Id{ /* PV额定无功功率协议配置项。 */
    Enum_Inv_Mfr_Id_t mfr_id;           /* 当前额定无功功率配置所属厂家编号。 */
    Inv_RegBlk_t Qn;                     /* PV额定无功功率只读寄存器配置。 */
} Inv_Qn_Id_t;
typedef struct Inv_set_volt_Id{ // 逆变器设定电压
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前设定电压配置所属厂家编号。 */
    Inv_RegBlk_t set_volt;     /* 逆变器设定电压只读寄存器配置。 */
}Inv_set_volt_Id_t;
typedef struct Inv_output_type_Id{ // 输出类型
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前输出类型配置所属厂家编号。 */
    Inv_RegBlk_t output_type;  /* 单相或三相等输出类型只读寄存器配置。 */
}Inv_output_type_Id_t;
typedef struct Inv_daily_energy_Id{ // 日发电量
    Enum_Inv_Mfr_Id_t mfr_id;       /* 当前日发电量配置所属厂家编号。 */
    Inv_RegBlk_t daily_energy;   /* 日发电量只读寄存器配置。 */
}Inv_daily_energy_Id_t;
// ----------------------------------------------------厂家控制类
typedef struct Inv_pwr_on_Id{ // 逆变器开机控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;          /* 当前开机控制配置所属厂家编号。 */
    Inv_CtrlDefaultRegBlk_t pwr_on;     /* 开机寄存器及固定默认写入值。 */
}Inv_pwr_on_Id_t;
typedef struct Inv_pwr_off_Id{ // 逆变器关机控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;          /* 当前关机控制配置所属厂家编号。 */
    Inv_CtrlDefaultRegBlk_t pwr_off;    /* 关机寄存器及固定默认写入值。 */
}Inv_pwr_off_Id_t;
typedef struct Inv_active_pwr_ctrl_Id{ // 有功功率数值控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;        /* 当前有功数值控制配置所属厂家编号。 */
    Inv_CtrlRegBlk_t active_pwr_ctrl; /* 有功功率数值写寄存器配置。 */
}Inv_active_pwr_ctrl_Id_t;
typedef struct Inv_reactive_pwr_ctrl_Id{ // 无功功率数值控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;          /* 当前无功数值控制配置所属厂家编号。 */
    Inv_CtrlRegBlk_t reactive_pwr_ctrl; /* 无功功率数值写寄存器配置。 */
}Inv_reactive_pwr_ctrl_Id_t;
typedef struct Inv_pwr_factor_ctrl_Id{ // 功率因数控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;       /* 当前功率因数控制配置所属厂家编号。 */
    Inv_CtrlRegBlk_t pwr_factor_ctrl; /* 功率因数写寄存器配置。 */
}Inv_pwr_factor_ctrl_Id_t;
typedef struct Inv_active_pwr_pct_ctrl_Id{ // 有功功率百分比控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;            /* 当前有功百分比控制配置所属厂家编号。 */
    Inv_CtrlRegBlk_t active_pwr_pct_ctrl; /* 有功功率百分比写寄存器配置。 */
}Inv_active_pwr_pct_ctrl_Id_t;
typedef struct Inv_reactive_pwr_pct_ctrl_Id{ // 无功功率百分比控制寄存器
    Enum_Inv_Mfr_Id_t mfr_id;              /* 当前无功百分比控制配置所属厂家编号。 */
    Inv_CtrlRegBlk_t reactive_pwr_pct_ctrl; /* 无功功率百分比写寄存器配置。 */
}Inv_reactive_pwr_pct_ctrl_Id_t;


// ---------------------------------------------------------------------------------------------------厂家信息
Inv_Mfr_Id_t g_inv_mfr[] = {    // 厂家信息
      //逆变器ID         厂家品牌                  规约版本
    {INV_MFR_SUNGROW_1, {"SUNGROW",                 0x0100}},   // 阳光电源
    {INV_MFR_HUAWEI_1,  {"HUAWEI",                  0x0300}},   // 华为V3.0
    {INV_MFR_HUAWEI_2,  {"HUAWEI",                  0x0200}},   // 华为V2.0
    {INV_MFR_GOODWE_1,  {"GOODWE_SMT/GT",           0x0100}},   // 固德威SMT60K G2/SMT80K/GT系列
    {INV_MFR_GOODWE_2,  {"GOODWE_SDT",              0x0100}},   // 固德威XS G3/DNS G3/MS G3/SDT G3/SDT G4系列
    {INV_MFR_GROWATT_1, {"GROWATT1",                0x0100}},   // 古瑞瓦特 MAX/MID/MAC系列
    {INV_MFR_GROWATT_2, {"GROWATT2",                0x0100}},
    {INV_MFR_SAJ_1,     {"SAJ_R5 PLUS",             0x0100}},   // 三晶R5 PLUS系列
    {INV_MFR_SAJ_2,     {"SAJ_R6/C6",               0x0100}},   // 三晶R6 17-50K和C6系列
    {INV_MFR_SAJ_3,     {"SAJ_R6",                  0x0100}},   // 三晶R6 3-15K系列
    {INV_MFR_JINLONG_1, {"JINLONG",                 0x0100}},   // 锦浪
    {INV_MFR_SOFAR_1,   {"SOFAR",                   0x0100}},   // 首航
    {INV_MFR_SINENG_1,  {"SINENG",                  0x0100}},   // 上能
    {INV_MFR_AUXSOL_1,  {"AUXSOL",                  0x0100}},

};


// ---------------------------------------------------------------------------------------------------厂家特征数据
Inv_Feature_Id_t g_inv_feature[] = {    // 厂家特征数据
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留   特征值下限(报文值)   特征值上限(报文值)
    {INV_MFR_SUNGROW_1,   {5035 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0,             495,                505}},      // 电网频率
    {INV_MFR_HUAWEI_1,    {40000,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     0,      0,       946684800,         3155759999}},      // 纪元秒,本地时间
    {INV_MFR_GOODWE_1,    {41313,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0,            3329,              25356}},      // 年月,高字节13~99,低字节1~12,组合范围为0x0D01~0x630C
    {INV_MFR_GOODWE_2,    {40313,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0,            3329,              25356}},      // 年月,高字节13~99,低字节1~12,组合范围为0x0D01~0x630C
    {INV_MFR_GROWATT_1,   {37,           1,            0x04,   TYPE_U16, Type_Byte_ABCD,     2,      0,            4950,               5050}},      // 电网频率,范围取49.5~50.5
    {INV_MFR_SAJ_1,       {273,          1,            0x03,   TYPE_I16, Type_Byte_ABCD,     1,      0,            10,                  900}},      // 温度,范围取1~90度
    {INV_MFR_SAJ_2,       {24636,        1,            0x03,   TYPE_I16, Type_Byte_ABCD,     1,      0,            10,                  900}},      // 温度,范围取1~90度
    {INV_MFR_SAJ_3,       {16384,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0,            1970,               2038}},      // 日期年,范围取1970~2038
    {INV_MFR_JINLONG_1,   {3074,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     0,      0,            1,                    12}},      // 日期月,范围取1~12
    {INV_MFR_SOFAR_1,     {1156,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0,            4950,               5050}},      // 电网频率,范围取49.5~50.5
    {INV_MFR_SINENG_1,    {40201,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0,            1970,               2038}},      // 日期年,范围取1970~2038
};


// ---------------------------------------------------------------------------------------------------数据类
Inv_ProtoData_Ua_Id_t g_inv_Ua[] = {    // A相电压寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5018 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32069,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {32069,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_2,    {30118,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {38,           1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {278,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_2,       {24610,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_3,       {16433,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_JINLONG_1,   {3033,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1165,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SINENG_1,    {31000,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},

};


Inv_ProtoData_Ub_Id_t g_inv_Ub[] = {    // B相电压寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5019 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32070,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {32070,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_2,    {30119,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {42,           1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {284,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_2,       {24616,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_3,       {16440,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_JINLONG_1,   {3034,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1176,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SINENG_1,    {31006,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},

};


Inv_ProtoData_Uc_Id_t g_inv_Uc[] = {    // C相电压寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5020 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32071,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {32071,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_2,    {30120,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {46,           1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {290,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_2,       {24622,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_3,       {16447,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_JINLONG_1,   {3035,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1187,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SINENG_1,    {31012,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},

};


Inv_ProtoData_Ia_Id_t g_inv_Ia[] = {    // A相电流寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5020 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32072,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {32072,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {30121,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {39,           1,            0x04,   TYPE_I16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {279,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_2,       {24611,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_3,       {16434,        1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_JINLONG_1,   {3036,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1166,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {31001,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Ib_Id_t g_inv_Ib[] = {    // B相电流寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5021 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32074,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {32074,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {30122,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {43,           1,            0x04,   TYPE_I16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {285,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_2,       {24617,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_3,       {16441,        1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_JINLONG_1,   {3037,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1177,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {31007,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Ic_Id_t g_inv_Ic[] = {    // C相电流寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5022 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32076,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {32076,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {30123,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {47,           1,            0x04,   TYPE_I16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {291,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_2,       {24623,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_3,       {16448,        1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_JINLONG_1,   {3038,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {11188,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {31013,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Pa_Id_t g_inv_Pa[] = {    // A相功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_GOODWE_1,   {32120 ,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,  {40,            2,            0x04,   TYPE_U32, Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SAJ_1,      {282,           1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_2,      {24614,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_3,      {16437,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,    {1167,          1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Pb_Id_t g_inv_Pb[] = {    // B相功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_GOODWE_1,   {32122 ,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,  {44,            2,            0x04,   TYPE_U32, Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SAJ_1,      {288,           1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_2,      {24620,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_3,      {16444,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,    {1178,          1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Pc_Id_t g_inv_Pc[] = {    // C相功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_GOODWE_1,   {32124 ,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,  {48,            2,            0x04,   TYPE_U32, Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SAJ_1,      {294,           1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_2,      {24626,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_3,      {16451,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,    {1189,          1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Pt_Id_t g_inv_Pt[] = {    // 总功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5030 ,        2,            0x04,   TYPE_I32, Type_Byte_CDAB,     3,      0}},
    {INV_MFR_HUAWEI_1,    {32080,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {32080,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {30127,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,   {35,           2,            0x04,   TYPE_U32, Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SAJ_1,       {275,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_2,       {24605,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_3,       {16551,        1,            0x03,   TYPE_I16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_JINLONG_1,   {3004,         2,            0x04,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,     {1157,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {31018,        2,            0x04,   TYPE_I32, Type_Byte_ABCD,     3,      0}},

};

Inv_ProtoData_Qa_Id_t g_inv_Qa[] = {    // A相无功功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SOFAR_1,     {1168,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Qb_Id_t g_inv_Qb[] = {    // B相无功功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SOFAR_1,     {1179,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Qc_Id_t g_inv_Qc[] = {    // C相无功功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SOFAR_1,     {1190,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},

};


Inv_ProtoData_Qt_Id_t g_inv_Qt[] = {    // 总无功功率寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5032 ,        2,            0x04,   TYPE_I32, Type_Byte_CDAB,     3,      0}},
    {INV_MFR_HUAWEI_1,    {32082,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {32082,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {30135,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,   {232,          2,            0x04,   TYPE_I32, Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SAJ_1,       {276,          1,            0x03,   TYPE_I16, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_2,       {24607,        2,            0x03,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_JINLONG_1,   {3055,         2,            0x04,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,     {1158,         1,            0x03,   TYPE_I16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {31020,        2,            0x04,   TYPE_I32, Type_Byte_ABCD,     3,      0}},

};


Inv_daily_energy_Id_t g_inv_daily_energy[] = {    // 日发电量寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5002 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {32114,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_GOODWE_1,    {32114,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_GOODWE_2,    {30144,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {53,           2,            0x04,   TYPE_U32, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {300,          1,            0x03,   TYPE_U16, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_2,       {24586,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SAJ_3,       {16599,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_JINLONG_1,   {3014,         1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SOFAR_1,     {1668,         2,            0x03,   TYPE_U32, Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {34025,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},

};

// -----------------------------------------------------------参数类数据-----------------------------
Inv_dev_no_Id_t g_inv_dev_no[] = {    // 设备编号或序列号
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
//    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};



Inv_Pn_Id_t g_inv_Pn[] = { /* PV额定有功功率协议配置表。 */
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5000 ,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {30073,        2,            0x03,   TYPE_U32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,   {6,            2,            0x03,   TYPE_U32, Type_Byte_ABCD,     4,      0}},

};


Inv_Qn_Id_t g_inv_Qn[] = { /* PV额定无功功率协议配置表。 */
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
//    {INV_MFR_SUNGROW_1,   {5000,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
};


Inv_set_volt_Id_t g_inv_set_volt[] = {    // 逆变器设定电压
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
//    {INV_MFR_SUNGROW_1,   {5000,        1,            0x04,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
};


Inv_output_type_Id_t g_inv_output_type[] = {    // 逆变器输出类型
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5001,        1,            0x04,   TYPE_U16, Type_Byte_CDAB,     0,      0}},
};

/*
Inv_pwr_status_Id_t g_inv_pwr_status[] = {    // 开关机状态
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5005 ,        1,            0x03,   TYPE_U16, Type_Byte_CDAB,     0,      0}},
    {INV_MFR_HUAWEI_1,    {40200,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},
    {INV_MFR_GOODWE_1,    {41330,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},
    {INV_MFR_GOODWE_2,    {40330,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},
    {INV_MFR_SAJ_1,       {4151,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},
    {INV_MFR_SAJ_2,       {4151,         1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},
    {INV_MFR_SAJ_3,       {13324,        1,            0x03,   TYPE_U16, Type_Byte_ABCD,     0,      0}},

};
*/

// --------------------------------------控制类数据------------------------------


Inv_pwr_on_Id_t g_inv_pwr_on[] = {    // 逆变器开机控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     写功能码  数据类型    字节序        小数位数  预留   默认值
    {INV_MFR_SUNGROW_1,   {5005 ,        1,            0x06,   TYPE_U16, Type_Byte_CDAB,     0,      0,      207}},
    {INV_MFR_HUAWEI_1,    {40200,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},
    {INV_MFR_GOODWE_1,    {41330,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_GOODWE_2,    {40330,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_GROWATT_1,   {0,            1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},
    {INV_MFR_SAJ_1,       {4151,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_2,       {4151,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_3,       {13324,        1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_JINLONG_1,   {3006,         1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      190}},
    {INV_MFR_SOFAR_1,     {4356,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},
    {INV_MFR_SINENG_1,    {42001,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},

};


Inv_pwr_off_Id_t g_inv_pwr_off[] = {    // 逆变器关机控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     写功能码  数据类型    字节序        小数位数  预留   默认值
    {INV_MFR_SUNGROW_1,   {5005 ,        1,            0x06,   TYPE_U16, Type_Byte_CDAB,     0,      0,      206}},
    {INV_MFR_HUAWEI_1,    {40201,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},
    {INV_MFR_GOODWE_1,    {41331,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_GOODWE_2,    {40331,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_GROWATT_1,   {0,            1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_SAJ_1,       {4151,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_2,       {4151,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_3,       {13324,        1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_JINLONG_1,   {3006,         1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      222}},
    {INV_MFR_SOFAR_1,     {4356,         1,            0x10,   TYPE_U16, Type_Byte_ABCD,     0,      0,      0}},
    {INV_MFR_SINENG_1,    {42001,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     0,      0,      1}},

};


Inv_active_pwr_ctrl_Id_t g_inv_active_pwr_ctrl[] = {    // 逆变器有功功率控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     写功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5038 ,        1,            0x06,   TYPE_U16, Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {40120,        1,            0x06,   TYPE_U16, Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {42404,        2,            0x10,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {42404,        2,            0x10,   TYPE_U32, Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SINENG_1,    {46050,        2,            0x10,   TYPE_I32, Type_Byte_ABCD,     3,      0}},
};


Inv_reactive_pwr_ctrl_Id_t g_inv_reactive_pwr_ctrl[] = {    // 逆变器无功功率控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5039 ,        1,            0x06,   TYPE_I16,  Type_Byte_CDAB,     1,      0}},
    {INV_MFR_GOODWE_1,    {42411,        2,            0x10,   TYPE_I32,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {42411,        2,            0x10,   TYPE_I32,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GROWATT_1,   {137,          2,            0x10,   TYPE_I32,  Type_Byte_ABCD,     4,      0}},
    {INV_MFR_SINENG_1,    {46052,        2,            0x10,   TYPE_I32,  Type_Byte_ABCD,     3,      0}},
};


Inv_pwr_factor_ctrl_Id_t g_inv_pwr_factor_ctrl[] = {    // 逆变器功率因数控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5018 ,        1,            0x06,   TYPE_I16,  Type_Byte_CDAB,     3,      0}},
    {INV_MFR_HUAWEI_1,    {40122,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_1,    {42413,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_GOODWE_2,    {42413,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SAJ_1,       {4126,         1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     3,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_2,       {4126,         1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     3,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_3,       {13335,        1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     3,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_JINLONG_1,   {3052,         1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     3,      0}},
    {INV_MFR_SOFAR_1,     {4361,         1,            0x10,   TYPE_I16,  Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SINENG_1,    {46056,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     4,      0}},
};


Inv_active_pwr_pct_ctrl_Id_t g_inv_active_pwr_pct_ctrl[] = {    // 有功功率百分比控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5007 ,        1,            0x06,   TYPE_U16,  Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {40125,        1,            0x06,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {42407,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_2,    {42407,        1,            0x06,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SAJ_1,       {4124,         1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_2,       {4124,         1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_SAJ_3,       {13323,        1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},   // 三晶逆变器控制功能码必须是0x10
    {INV_MFR_GROWATT_1,   {3,            1,            0x06,   TYPE_U16,  Type_Byte_ABCD,     0,      0}},
    {INV_MFR_JINLONG_1,   {3051,         1,            0x06,   TYPE_U16,  Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SOFAR_1,     {4358,         1,            0x10,   TYPE_U16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SINENG_1,    {46054,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     2,      0}},
};



Inv_reactive_pwr_pct_ctrl_Id_t g_inv_reactive_pwr_pct_ctrl[] = {    // 无功功率百分比控制寄存器
    //逆变器ID           寄存器地址   寄存器个数     读功能码  数据类型    字节序        小数位数  预留
    {INV_MFR_SUNGROW_1,   {5036 ,        1,            0x06,   TYPE_I16,  Type_Byte_CDAB,     1,      0}},
    {INV_MFR_HUAWEI_1,    {40123,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_1,    {42414,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GOODWE_2,    {42414,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_GROWATT_1,   {4,            1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     0,      0}},
    {INV_MFR_JINLONG_1,   {3050,         1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     2,      0}},
    {INV_MFR_SOFAR_1,     {4360,         1,            0x10,   TYPE_I16,  Type_Byte_ABCD,     1,      0}},
    {INV_MFR_SINENG_1,    {46055,        1,            0x06,   TYPE_I16,  Type_Byte_ABCD,     2,      0}},
};


#define INV_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

/* 在目标厂家对应的分项表中查找配置；没有匹配项时保留初始化的0xFFFF地址。 */
#define INV_COPY_MFR_POINT(target, table, member)                                      \
    do {                                                                               \
        uint16_t table_index;                                                          \
        for(table_index = 0U; table_index < INV_ARRAY_COUNT(table); ++table_index) {   \
            if((table)[table_index].mfr_id == mfr_id) {                                \
                rt_memcpy(&(target), &(table)[table_index].member, sizeof(target));     \
                break;                                                                 \
            }                                                                          \
        }                                                                              \
    } while(0)

/* 将一条协议的全部可选点初始化为不支持。地址字段为16位，因此全0xFF表示为0xFFFF。 */
static void inv_proto_set_all_points_unused(Inv_Proto_t *protocol)
{
    uint8_t index;

    rt_memset(protocol, 0, sizeof(*protocol));
    protocol->feature.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;

    for(index = 0U; index < ENUM_PHASE_MAX; ++index) {
        protocol->data.Ux[index].reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
        protocol->data.Ix[index].reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    }
    for(index = 0U; index < ENUM_PMAX; ++index) {
        protocol->data.Px[index].reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    }
    for(index = 0U; index < ENUM_QMAX; ++index) {
        protocol->data.Qx[index].reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    }
    for(index = 0U; index < ENUM_PFMAX; ++index) {
        protocol->data.PFx[index].reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    }

    protocol->param.dev_no.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->param.Pn.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->param.Qn.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->param.set_volt.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->param.output_type.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;

    protocol->ctrl.pwr_on.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.pwr_off.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.active_pwr_ctrl.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.reactive_pwr_ctrl.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.pwr_factor_ctrl.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.active_pwr_pct_ctrl.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->ctrl.reactive_pwr_pct_ctrl.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
    protocol->daily_energy.reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED;
}

/* 按厂家枚举逐项查询各配置表，并组装到RAM协议库的同编号槽位。 */
void inv_proto_default_lib_init(void)
{
    Enum_Inv_Mfr_Id_t mfr_id;

    rt_memset(&g_inv_proto_lib, 0, sizeof(g_inv_proto_lib));

    for(mfr_id = INV_MFR_SUNGROW_1; mfr_id < INV_MFR_MAX; ++mfr_id) {
        Inv_Proto_t *protocol = &g_inv_proto_lib.proto[(uint16_t)mfr_id];
        uint16_t mfr_index;
        uint8_t mfr_found = 0U;

        inv_proto_set_all_points_unused(protocol);

        /* 厂家信息是协议有效的必要条件；枚举存在但厂家表缺项时该槽位保持无效。 */
        for(mfr_index = 0U; mfr_index < INV_ARRAY_COUNT(g_inv_mfr); ++mfr_index) {
            if(g_inv_mfr[mfr_index].mfr_id == mfr_id) {
                rt_memcpy(&protocol->mfr_info, &g_inv_mfr[mfr_index].mfr_info,
                          sizeof(protocol->mfr_info));
                mfr_found = 1U;
                break;
            }
        }
        if(mfr_found == 0U) {
            continue;
        }

        INV_COPY_MFR_POINT(protocol->feature, g_inv_feature, feature);

        INV_COPY_MFR_POINT(protocol->data.Ux[ENUM_UA], g_inv_Ua, Ua);
        INV_COPY_MFR_POINT(protocol->data.Ux[ENUM_UB], g_inv_Ub, Ub);
        INV_COPY_MFR_POINT(protocol->data.Ux[ENUM_UC], g_inv_Uc, Uc);
        INV_COPY_MFR_POINT(protocol->data.Ix[ENUM_IA], g_inv_Ia, Ia);
        INV_COPY_MFR_POINT(protocol->data.Ix[ENUM_IB], g_inv_Ib, Ib);
        INV_COPY_MFR_POINT(protocol->data.Ix[ENUM_IC], g_inv_Ic, Ic);
        INV_COPY_MFR_POINT(protocol->data.Px[ENUM_PA], g_inv_Pa, Pa);
        INV_COPY_MFR_POINT(protocol->data.Px[ENUM_PB], g_inv_Pb, Pb);
        INV_COPY_MFR_POINT(protocol->data.Px[ENUM_PC], g_inv_Pc, Pc);
        INV_COPY_MFR_POINT(protocol->data.Px[ENUM_PT], g_inv_Pt, Pt);
        INV_COPY_MFR_POINT(protocol->data.Qx[ENUM_QA], g_inv_Qa, Qa);
        INV_COPY_MFR_POINT(protocol->data.Qx[ENUM_QB], g_inv_Qb, Qb);
        INV_COPY_MFR_POINT(protocol->data.Qx[ENUM_QC], g_inv_Qc, Qc);
        INV_COPY_MFR_POINT(protocol->data.Qx[ENUM_QT], g_inv_Qt, Qt);

        /* 当前配置文件尚无功率因数读取表，PFx保持0xFFFF，后续增加表后在此接入。 */
        INV_COPY_MFR_POINT(protocol->daily_energy, g_inv_daily_energy, daily_energy);
        INV_COPY_MFR_POINT(protocol->param.dev_no, g_inv_dev_no, dev_no);
        INV_COPY_MFR_POINT(protocol->param.Pn, g_inv_Pn, Pn);
        INV_COPY_MFR_POINT(protocol->param.Qn, g_inv_Qn, Qn);
        INV_COPY_MFR_POINT(protocol->param.set_volt, g_inv_set_volt, set_volt);
        INV_COPY_MFR_POINT(protocol->param.output_type, g_inv_output_type, output_type);

        INV_COPY_MFR_POINT(protocol->ctrl.pwr_on, g_inv_pwr_on, pwr_on);
        INV_COPY_MFR_POINT(protocol->ctrl.pwr_off, g_inv_pwr_off, pwr_off);
        INV_COPY_MFR_POINT(protocol->ctrl.active_pwr_ctrl,
                           g_inv_active_pwr_ctrl, active_pwr_ctrl);
        INV_COPY_MFR_POINT(protocol->ctrl.reactive_pwr_ctrl,
                           g_inv_reactive_pwr_ctrl, reactive_pwr_ctrl);
        INV_COPY_MFR_POINT(protocol->ctrl.pwr_factor_ctrl,
                           g_inv_pwr_factor_ctrl, pwr_factor_ctrl);
        INV_COPY_MFR_POINT(protocol->ctrl.active_pwr_pct_ctrl,
                           g_inv_active_pwr_pct_ctrl, active_pwr_pct_ctrl);
        INV_COPY_MFR_POINT(protocol->ctrl.reactive_pwr_pct_ctrl,
                           g_inv_reactive_pwr_pct_ctrl, reactive_pwr_pct_ctrl);

        g_inv_proto_lib.valid[(uint16_t)mfr_id] = INVERTER_PROTOCOL_VALID;
    }
}

#undef INV_COPY_MFR_POINT
#undef INV_ARRAY_COUNT
