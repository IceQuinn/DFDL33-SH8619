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




Inv_Proto_t g_inv_proto_default_lib[4] = {
    // 厂家一：阳光
    {
        .mfr_info = {   // 厂家信息
            .name = "SUNGROW",
            .proto_ver = {1U, 0U}
        },
        .feature = {    // 厂家特征数据
            .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
            .reg_cnt = 0U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 0U,
            .feature_val = 0U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
        }
    },
    // 厂家二：华为
    {
        .mfr_info = {   // 厂家信息
            .name = "HUAWEI",
            .proto_ver = {1U, 0U}
        },
        .feature = {    // 厂家特征数据
            .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
            .reg_cnt = 0U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 0U,
            .feature_val = 0U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
        }
    },
    // 厂家三：固德威
    {
        .mfr_info = {   // 厂家信息
            .name = "GOODWE",
            .proto_ver = {1U, 0U}
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 0x75AC,
            .reg_cnt = 1U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 2U,
            .feature_val = -1,
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 0x75A6,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电压寄存器
                    .reg_addr = 0x75A7,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电压寄存器
                    .reg_addr = 0x75A8,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 0x75A9,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电流寄存器
                    .reg_addr = 0x75AA,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电流寄存器
                    .reg_addr = 0x75AB,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = 0x75AF,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = 0x75B7,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 0x8131,
                .read_func_code = 0x03U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
        }
    },
    // 厂家四：锦浪
    {
        .mfr_info = {   // 厂家信息
            .name = "JINWANG",
            .proto_ver = {1U, 0U}
        },
        .feature = {    // 厂家特征数据
            .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
            .reg_cnt = 0U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 0U,
            .feature_val = 0U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .read_func_code = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,
                .write_default_val = 0U
            },
        }
    },
};
