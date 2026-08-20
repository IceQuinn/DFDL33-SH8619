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

// typedef enum Enum_Inv_Mfr_Id
// {
//     INV_MFR_SUNGROW_1 = 0,
//     INV_MFR_HUAWEI_1,
//     INV_MFR_GOODWE_1,
//     INV_MFR_JINWANG_1,
// }Enum_Inv_Mfr_Id_t;

// typedef Inv_Mfr{
//     Enum_Inv_Mfr_Id_t mfr_id;
//     Inv_MfrInfo_t mfr_info;
// }Inv_Mfr_t;

// Inv_Mfr_t g_inv_mfr[] = {
//     {INV_MFR_SUNGROW_1, {0, 0}},
// }



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
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 1,

            .default_val = 5000
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 5018,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 5019,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 5020,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 5021,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 5022,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 5023,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 5030,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 5032,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 5034,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 5000,
                .reg_cnt = 1,
                .read_func_code = 0x04,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 5048,
                .reg_cnt = 1,
                .read_func_code = 0x04,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0xCF
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0xCE
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 5018,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 5007,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 5036,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 0,

            .default_val = 2,
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 32069,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 32070,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 32071,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 32072,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                },
                {   // B相电流寄存器
                    .reg_addr = 32074,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                },
                {   // C相电流寄存器
                    .reg_addr = 32076,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 32080,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 32082,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 32084,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 30073,
                .reg_cnt = 2,
                .read_func_code = 0x03,
                .data_type = TYPE_U32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 30079,
                .reg_cnt = 2,
                .read_func_code = 0x03,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 40200,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 40201,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 40126,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 40129,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 40122,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 40125,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 40123,
                .reg_cnt = 2,
                .write_func_code = 0x10,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 2,

            .default_val = 5000
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 0x75A6,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 0x75A7,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 0x75A8,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 0x75A9,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 0x75AA,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 0x75AB,
                    .reg_cnt = 1,
                    .read_func_code = 0x03,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 0x75AF,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 0x75B7,
                    .reg_cnt = 2,
                    .read_func_code = 0x03,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 0x8131,
                .reg_cnt = 1,
                .read_func_code = 0x03,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .write_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 2,

            .default_val = 5000
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 3033,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电压寄存器
                    .reg_addr = 3034,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电压寄存器
                    .reg_addr = 3035,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 3036,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // B相电流寄存器
                    .reg_addr = 3037,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                },
                {   // C相电流寄存器
                    .reg_addr = 3038,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总有功功率寄存器
                    .reg_addr = 3004,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总无功功率寄存器
                    .reg_addr = 3055,
                    .reg_cnt = 2,
                    .read_func_code = 0x04,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0,
                    .read_func_code = 0,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0
                },
                {   // 总功率因数寄存器
                    .reg_addr = 3059,
                    .reg_cnt = 1,
                    .read_func_code = 0x04,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
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
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 3044,
                .reg_cnt = 2,
                .read_func_code = 0x04,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 3046,
                .reg_cnt = 2,
                .read_func_code = 0x04,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0,
                .read_func_code = 0,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0xBE
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0,

                .write_default_val = 0xDE
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 3080,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 3082,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 3053,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 3051,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 3050,
                .reg_cnt = 1,
                .write_func_code = 0x06,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2
            },
        }
    },
};
