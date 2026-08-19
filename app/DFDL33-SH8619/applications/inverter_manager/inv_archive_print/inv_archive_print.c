/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-13     mutou       the first version
 */
#include "inv_archive_print.h"

#include "inverter_archive.h"

#include <rtthread.h>

/* 将档案端口编号转换为便于日志查看的端口名称。 */
static const char *inv_archive_port_name(uint8_t port)
{
    /* 每个有效端口返回固定名称，未知端口统一返回UNKNOWN。 */
    switch(port) {
    case INV_PORT_RJ45_1:
        return "RJ45-I";

    case INV_PORT_RJ45_2:
        return "RJ45-II";

    case INV_PORT_RS485_2:
        return "RS485-II";

    case INV_PORT_WIRELESS:
        return "WIRELESS";

    default:
        return "UNKNOWN";
    }
}

/* 打印档案库统计信息及全部固定档案槽位。 */
void Inv_Archive_Print(void)
{
    uint8_t index;
    uint8_t valid_count = 0U;

    /* 打印前重新统计有效槽位，便于对比档案库中保存的count。 */
    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index) {
        /* 当前槽位有效时累计实际有效数量。 */
        if(g_inv_archive_lib.valid[index] == INVERTER_ARCHIVE_VALID) {
            ++valid_count;
        }
    }

    rt_kprintf("[%08d] Archive library: valid count=%d, stored count=%d, capacity=%d\n", rt_tick_get(), valid_count, g_inv_archive_lib.count, INVERTER_ARCHIVE_MAX_COUNT);
    rt_kprintf("[%08d] %-5s %-11s %-15s %-32s %-13s %-18s\n", rt_tick_get(), "No.", "Valid", "Modbus address", "Manufacturer", "Protocol ver", "Access port");

    /* 逐槽位打印档案内容，无效槽位也保留输出以便检查Flash数据。 */
    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index) {
        const Inv_Archive_t *archive = &g_inv_archive_lib.archives[index];
        const char *manufacturer_text;
        const char *valid_text;
        uint8_t valid = g_inv_archive_lib.valid[index];
        char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U];
        char modbus_address[16];

        Inv_Archive_Copy_Mfr_Name(manufacturer, archive->mfr_info.name);
        rt_snprintf(modbus_address, sizeof(modbus_address), "%d", archive->mb_addr);

        /* 厂家名称为空时使用固定文本，避免表格中该字段完全空白。 */
        if(manufacturer[0] == '\0') {
            manufacturer_text = "(empty)";
        }
        else {
            manufacturer_text = manufacturer;
        }

        /* 有效标志转换为文本，同时保留后面的数值便于排查异常标志。 */
        if(valid == INVERTER_ARCHIVE_VALID) {
            valid_text = "VALID";
        }
        else {
            valid_text = "INVALID";
        }

        rt_kprintf("[%08d] %-5d %-7s(%d)  %-15s %-32s %d.%d          %d(%s)\n", rt_tick_get(), index + 1, valid_text, valid, modbus_address, manufacturer_text, archive->mfr_info.proto_ver[0], archive->mfr_info.proto_ver[1], archive->port, inv_archive_port_name(archive->port));
    }

    rt_kprintf("[%08d] Printed %d archive slots, valid count=%d\n", rt_tick_get(), INVERTER_ARCHIVE_MAX_COUNT, valid_count);
}

/* FinSH命令入口，调用统一档案打印接口。 */
static int inv_archive_print(void)
{
    Inv_Archive_Print();
    return 0;
}
MSH_CMD_EXPORT(inv_archive_print, print all inverter archive slots);
