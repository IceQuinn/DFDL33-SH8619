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

typedef enum Enum_Inv_Mfr_Id
{
    INV_MFR_SUNGROW_1 = 0, /* 阳光电源默认协议在各分项配置数组中的关联编号。 */
    INV_MFR_HUAWEI_1,     /* 华为默认协议在各分项配置数组中的关联编号。 */
    INV_MFR_GOODWE_1,     /* 固德威默认协议在各分项配置数组中的关联编号。 */
    INV_MFR_JINWANG_1,    /* 锦浪默认协议在各分项配置数组中的关联编号。 */
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
typedef struct Inv_pv_rated_active_pwr_Id{ // PV额定有功功率
    Enum_Inv_Mfr_Id_t mfr_id;         /* 当前额定有功功率配置所属厂家编号。 */
    Inv_RegBlk_t pv_rated_active_pwr;  /* PV额定有功功率只读寄存器配置。 */
}Inv_pv_rated_active_pwr_Id_t;
typedef struct Inv_pv_rated_reactive_pwr_Id{ // PV额定无功功率
    Enum_Inv_Mfr_Id_t mfr_id;           /* 当前额定无功功率配置所属厂家编号。 */
    Inv_RegBlk_t pv_rated_reactive_pwr;  /* PV额定无功功率只读寄存器配置。 */
}Inv_pv_rated_reactive_pwr_Id_t;
typedef struct Inv_set_volt_Id{ // 逆变器设定电压
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前设定电压配置所属厂家编号。 */
    Inv_RegBlk_t set_volt;     /* 逆变器设定电压只读寄存器配置。 */
}Inv_set_volt_Id_t;
typedef struct Inv_output_type_Id{ // 输出类型
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前输出类型配置所属厂家编号。 */
    Inv_RegBlk_t output_type;  /* 单相或三相等输出类型只读寄存器配置。 */
}Inv_output_type_Id_t;
typedef struct Inv_pwr_status_Id{ // 开关机状态
    Enum_Inv_Mfr_Id_t mfr_id; /* 当前开关机状态配置所属厂家编号。 */
    Inv_RegBlk_t pwr_status;   /* 开关机状态只读寄存器配置。 */
}Inv_pwr_status_Id_t;
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
    {INV_MFR_SUNGROW_1, {"SUNGROW", 0x0100}},
    {INV_MFR_HUAWEI_1,  {"HUAWEI",  0x0100}},
    {INV_MFR_GOODWE_1,  {"GOODWE",  0x0100}},
    {INV_MFR_JINWANG_1, {"JINWANG", 0x0100}},
};
// ---------------------------------------------------------------------------------------------------厂家特征数据
Inv_Feature_Id_t g_inv_feature[] = {    // 厂家特征数据
    //逆变器ID,     寄存器地址，寄存器个数，读功能码，数据类型, 字节序, 小数位数, 预留，下限，上限
    {INV_MFR_SUNGROW_1, {5036, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0, 4500, 5500}},
    {INV_MFR_HUAWEI_1,  {32085, 1, 0x03, TYPE_U16, Type_Byte_ABCD, 0, 0, 2, 2}},
    {INV_MFR_GOODWE_1,  {0x75AC, 1, 0x03, TYPE_U16, Type_Byte_ABCD, 2, 0, 4500, 5500}},
    {INV_MFR_JINWANG_1, {3043, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 2, 0, 4500, 5500}},
};
// ---------------------------------------------------------------------------------------------------数据类
Inv_ProtoData_Ua_Id_t g_inv_Ua[] = {    // A相电压寄存器
    {INV_MFR_SUNGROW_1, {5018, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
    {INV_MFR_HUAWEI_1,  {5018, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Ub_Id_t g_inv_Ub[] = {    // B相电压寄存器
    {INV_MFR_SUNGROW_1, {5019, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
    {INV_MFR_HUAWEI_1,  {5019, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Uc_Id_t g_inv_Uc[] = {    // C相电压寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
    {INV_MFR_HUAWEI_1,  {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Ia_Id_t g_inv_Ia[] = {    // A相电流寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Ib_Id_t g_inv_Ib[] = {    // B相电流寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Ic_Id_t g_inv_Ic[] = {    // C相电流寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};

Inv_ProtoData_Pa_Id_t g_inv_Pa[] = {    // A相功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Pb_Id_t g_inv_Pb[] = {    // B相功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Pc_Id_t g_inv_Pc[] = {    // C相功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Pt_Id_t g_inv_Pt[] = {    // 总功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};

Inv_ProtoData_Qa_Id_t g_inv_Qa[] = {    // A相无功功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Qb_Id_t g_inv_Qb[] = {    // B相无功功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Qc_Id_t g_inv_Qc[] = {    // C相无功功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
Inv_ProtoData_Qt_Id_t g_inv_Qt[] = {    // 总无功功率寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},

};
// ---------------------------------------------------------------------------------------------------参数类数据
Inv_dev_no_Id_t g_inv_dev_no[] = {    // 设备编号或序列号
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_pv_rated_active_pwr_Id_t g_inv_pv_rated_active_pwr[] = {    // PV额定有功功率
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_pv_rated_reactive_pwr_Id_t g_inv_pv_rated_reactive_pwr[] = {    // PV额定无功功率
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_set_volt_Id_t g_inv_set_volt[] = {    // 逆变器设定电压
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_output_type_Id_t g_inv_output_type[] = {    // 逆变器输出类型
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_pwr_status_Id_t g_inv_pwr_status[] = {    // 开关机状态
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
// ---------------------------------------------------------------------------------------------------控制类数据
Inv_pwr_on_Id_t g_inv_pwr_on[] = {    // 逆变器开机控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0, 0x00}},
};
Inv_pwr_off_Id_t g_inv_pwr_off[] = {    // 逆变器关机控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0, 0x01}},
};
Inv_active_pwr_ctrl_Id_t g_inv_active_pwr_ctrl[] = {    // 逆变器有功功率控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_reactive_pwr_ctrl_Id_t g_inv_reactive_pwr_ctrl[] = {    // 逆变器无功功率控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_pwr_factor_ctrl_Id_t g_inv_pwr_factor_ctrl[] = {    // 逆变器功率因数控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_active_pwr_pct_ctrl_Id_t g_inv_active_pwr_pct_ctrl[] = {    // 有功功率百分比控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};
Inv_reactive_pwr_pct_ctrl_Id_t g_inv_reactive_pwr_pct_ctrl[] = {    // 无功功率百分比控制寄存器
    {INV_MFR_SUNGROW_1, {5020, 1, 0x04, TYPE_U16, Type_Byte_ABCD, 1, 0}},
};




Inv_Proto_t g_inv_proto_default_lib[4] = {
    // 厂家一：阳光
    {
        .mfr_info = {   // 厂家信息
            .name = "SUNGROW",
            .proto_ver = 0x0100
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 5036,
            .reg_cnt = 1,
            .read_func_code = 0x04,
            .data_type = TYPE_U16,
            .byte_order = Type_Byte_ABCD,
            .decimal_places = 1,

            .lower_limit = 4500,
            .upper_limit = 5500
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 5018,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 5019,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 5020,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 5021,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 5022,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 5023,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 5030,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 5032,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 5034,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 5000,
                .reg_cnt = 1,
                .read_func_code = 0x04,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 5048,
                .reg_cnt = 1,
                .read_func_code = 0x04,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0xCF
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0xCE
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 5018,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 5007,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 1
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 5036,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 1
            },
        },
    },
    // 厂家二：华为
    {
        .mfr_info = {   // 厂家信息
            .name = "HUAWEI",
            .proto_ver = 0x0100
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 32085,
            .reg_cnt = 1,
            .read_func_code = 0x03,
            .data_type = TYPE_U16,
            .byte_order = Type_Byte_ABCD,
            .decimal_places = 0,

            .lower_limit = 2,
            .upper_limit = 2,
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 32069,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 32070,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 32071,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 32072,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                },
                {   // B相电流寄存器
                    .reg_addr = 32074,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                },
                {   // C相电流寄存器
                    .reg_addr = 32076,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 32080,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 32082,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 32084,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3,

                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 30073,
                .reg_cnt = 2,
                .read_func_code = 0x03,
                .data_type = TYPE_U32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 30079,
                .reg_cnt = 2,
                .read_func_code = 0x03,
                .data_type = TYPE_I32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 40200,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 40201,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 40126,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 40129,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_I32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 40122,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 40125,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 2
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 40123,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 2
            },
        }
    },
    // 厂家三：固德威
    {
        .mfr_info = {   // 厂家信息
            .name = "GOODWE",
            .proto_ver = 0x0100
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 0x75AC,
            .reg_cnt = 1,
            .read_func_code = 0x03,
            .data_type = TYPE_U16,
            .byte_order = Type_Byte_ABCD,
            .decimal_places = 2,

            .lower_limit = 4500,
            .upper_limit = 5500
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 0x75A6,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 0x75A7,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 0x75A8,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 0x75A9,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 0x75AA,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 0x75AB,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 0x75AF,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 0x75B7,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 0x8131,
                .reg_cnt = 1,
                .read_func_code = 0x03,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 40330,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 1234
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 40331,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 4321
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 259,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_I32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 256,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
        }
    },
    // 厂家四：锦浪
    {
        .mfr_info = {   // 厂家信息
            .name = "JINWANG",
            .proto_ver = 0x0100
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 3043,
            .reg_cnt = 1,
            .read_func_code = 0x04,
            .data_type = TYPE_U16,
            .byte_order = Type_Byte_ABCD,
            .decimal_places = 2,

            .lower_limit = 4500,
            .upper_limit = 5500
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 3033,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 3034,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 3035,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 3036,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 3037,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 3038,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 3004,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 3055,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 3059,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = Type_Byte_ABCD,
                    .decimal_places = 3
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 3044,
                .reg_cnt = 2,
                .read_func_code = 0x04,
                .data_type = TYPE_I32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 3046,
                .reg_cnt = 2,
                .read_func_code = 0x04,
                .data_type = TYPE_I32,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0xBE
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 0,

                .write_default_val = 0xDE
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 3080,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 1
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 3082,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 1
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 3053,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 3051,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 2
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 3050,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = Type_Byte_ABCD,
                .decimal_places = 2
            },
        }
    },
};
