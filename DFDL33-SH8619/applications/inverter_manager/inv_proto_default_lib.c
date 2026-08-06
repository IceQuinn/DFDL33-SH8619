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
        .data = {
            .volt = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .curr = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .active_pwr = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .reactive_pwr = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .pwr_factor = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        }
    },
    // 厂家二：华为
    {
        .mfr_info = {   // 厂家信息
            .name = "HUAWEI",
            .proto_ver = {1U, 0U}
        },
        .data = {
            .volt = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .curr = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .active_pwr = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .reactive_pwr = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .pwr_factor = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        }
    },
    // 厂家三：固德威
    {
        .mfr_info = {   // 厂家信息
            .name = "GEWEI",
            .proto_ver = {1U, 0U}
        },
        .data = {
            .volt = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .curr = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .active_pwr = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .reactive_pwr = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .pwr_factor = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
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
        .data = {
            .volt = {
                {   // A相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电压寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .curr = {
                {   // A相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相电流寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .active_pwr = {
                {   // A相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相有功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .reactive_pwr = {
                {   // A相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相无功功率寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
            .pwr_factor = {
                {   // A相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // B相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                },
                {   // C相功率因数寄存器
                    .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                    .reg_cnt = 0U,
                    .data_type = INVERTER_DATA_TYPE_UINT16,
                    .byte_order = INVERTER_BYTE_ORDER_AB,
                    .decimal_places = 0U
                }
            },
        },
        .param = {
            .dev_no = { // 设备编号寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_active_pwr = {    // PV额定有功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pv_rated_reactive_pwr = {    // PV额定无功功率寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .set_volt = {    // 设置电压寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .output_type = {    // 输出类型寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_status = {    // 开关机状态寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        },
        .ctrl = {
            .pwr_on = {    // 开机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_off = {    // 关机控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_ctrl = {    // 有功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_ctrl = {    // 无功功率数值控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .pwr_factor_ctrl = {    // 功率因数控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .active_pwr_pct_ctrl = {    // 有功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
            .reactive_pwr_pct_ctrl = {    // 无功功率百分比控制寄存器
                .reg_addr = INVERTER_PROTOCOL_REGISTER_UNUSED,
                .reg_cnt = 0U,
                .data_type = INVERTER_DATA_TYPE_UINT16,
                .byte_order = INVERTER_BYTE_ORDER_AB,
                .decimal_places = 0U
            },
        }
    },
};
