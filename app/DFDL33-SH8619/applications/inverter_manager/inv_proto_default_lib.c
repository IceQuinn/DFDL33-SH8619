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
            .reg_addr = 5036,
            .reg_cnt = 1U,
            .read_func_code = 0x04U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 1U,

            .default_val = 5000U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 5018,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电压寄存器
                    .reg_addr = 5019,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电压寄存器
                    .reg_addr = 5020,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 5021,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电流寄存器
                    .reg_addr = 5022,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电流寄存器
                    .reg_addr = 5023,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总有功功率寄存器
                    .reg_addr = 5030,
                    .reg_cnt = 2U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总无功功率寄存器
                    .reg_addr = 5032,
                    .reg_cnt = 2U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总功率因数寄存器
                    .reg_addr = 5034,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 5000,
                .reg_cnt = 1U,
                .read_func_code = 0x04U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 5048,
                .reg_cnt = 1U,
                .read_func_code = 0x04U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0xCFU
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 5005,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0xCEU
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 5018,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 5007,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 5036,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1U
            },
        },
    },
    // 厂家二：华为
    {
        .mfr_info = {   // 厂家信息
            .name = "HUAWEI",
            .proto_ver = {1U, 0U}
        },
        .feature = {    // 厂家特征数据
            .reg_addr = 32085,
            .reg_cnt = 1U,
            .read_func_code = 0x03U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 0U,

            .default_val = 2U,
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 32069,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电压寄存器
                    .reg_addr = 32070,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电压寄存器
                    .reg_addr = 32071,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 32072,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                },
                {   // B相电流寄存器
                    .reg_addr = 32074,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                },
                {   // C相电流寄存器
                    .reg_addr = 32076,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总有功功率寄存器
                    .reg_addr = 32080,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总无功功率寄存器
                    .reg_addr = 32082,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总功率因数寄存器
                    .reg_addr = 32084,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U,

                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 30073,
                .reg_cnt = 2U,
                .read_func_code = 0x03U,
                .data_type = TYPE_U32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 30079,
                .reg_cnt = 2U,
                .read_func_code = 0x03U,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 40200,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 40201,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 40126,
                .reg_cnt = 2U,
                .write_func_code = 0x10U,
                .data_type = TYPE_U32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 40129,
                .reg_cnt = 2U,
                .write_func_code = 0x10U,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 40122,
                .reg_cnt = 2U,
                .write_func_code = 0x10U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 40125,
                .reg_cnt = 2U,
                .write_func_code = 0x10U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 40123,
                .reg_cnt = 2U,
                .write_func_code = 0x10U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2U
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
            .read_func_code = 0x03U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 2U,

            .default_val = 5000U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 0x75A6,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电压寄存器
                    .reg_addr = 0x75A7,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电压寄存器
                    .reg_addr = 0x75A8,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 0x75A9,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电流寄存器
                    .reg_addr = 0x75AA,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电流寄存器
                    .reg_addr = 0x75AB,
                    .reg_cnt = 1U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总有功功率寄存器
                    .reg_addr = 0x75AF,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总无功功率寄存器
                    .reg_addr = 0x75B7,
                    .reg_cnt = 2U,
                    .read_func_code = 0x03U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
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
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 0x8131,
                .reg_cnt = 1U,
                .read_func_code = 0x03U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .write_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
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
            .reg_addr = 3043,
            .reg_cnt = 1U,
            .read_func_code = 0x04U,
            .data_type = TYPE_U16,
            .byte_order = INVERTER_BYTE_ORDER_NORMAL,
            .decimal_places = 2U,

            .default_val = 5000U
        },
        .data = {
            .Ux = {
                {   // A相电压寄存器
                    .reg_addr = 3033,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电压寄存器
                    .reg_addr = 3034,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电压寄存器
                    .reg_addr = 3035,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Ix = {
                {   // A相电流寄存器
                    .reg_addr = 3036,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // B相电流寄存器
                    .reg_addr = 3037,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                },
                {   // C相电流寄存器
                    .reg_addr = 3038,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 1U
                }
            },
            .Px = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总有功功率寄存器
                    .reg_addr = 3004,
                    .reg_cnt = 2U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .Qx = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总无功功率寄存器
                    .reg_addr = 3055,
                    .reg_cnt = 2U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_I32,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                }
            },
            .PFx = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .read_func_code = 0U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 0U
                },
                {   // 总功率因数寄存器
                    .reg_addr = 3059,
                    .reg_cnt = 1U,
                    .read_func_code = 0x04U,
                    .data_type = TYPE_U16,
                    .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                    .decimal_places = 3U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = 3044,
                .reg_cnt = 2U,
                .read_func_code = 0x04U,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = 3046,
                .reg_cnt = 2U,
                .read_func_code = 0x04U,
                .data_type = TYPE_I32,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .read_func_code = 0U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0xBEU
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = 3006,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 0U,

                .write_default_val = 0xDEU
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = 3080,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = 3082,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 1U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = 3053,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 3U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = 3051,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = 3050,
                .reg_cnt = 1U,
                .write_func_code = 0x06U,
                .data_type = TYPE_U16,
                .byte_order = INVERTER_BYTE_ORDER_NORMAL,
                .decimal_places = 2U
            },
        }
    },
};
