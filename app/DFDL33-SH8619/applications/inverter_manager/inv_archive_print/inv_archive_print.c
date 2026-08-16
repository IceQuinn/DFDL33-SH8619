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

static void inv_archive_copy_mfr_name(
    char output[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U],
    const char input[INVERTER_ARCHIVE_BRAND_WIRE_SIZE])
{
    uint8_t index;

    for (index = 0U; index < INVERTER_ARCHIVE_BRAND_WIRE_SIZE; ++index)
    {
        uint8_t character = (uint8_t)input[index];

        if ((character == 0U) || (character == 0xFFU))
        {
            break;
        }

        output[index] = ((character >= 0x20U) && (character <= 0x7EU))
                            ? (char)character
                            : '?';
    }
    output[index] = '\0';
}

static const char *inv_archive_port_name(uint8_t port)
{
    switch (port)
    {
    case INV_PORT_RJ45_1:    return "RJ45-I";
    case INV_PORT_RJ45_2:    return "RJ45-II";
    case INV_PORT_RS485_2:   return "RS485-II";
    case INV_PORT_WIRELESS:  return "WIRELESS";
    default:                 return "UNKNOWN";
    }
}

void Inv_Archive_Print(void)
{
    uint8_t index;
    uint8_t valid_count = 0U;

    for (index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index)
    {
        if (g_inv_archive_lib.slots[index].valid == INVERTER_ARCHIVE_VALID)
        {
            ++valid_count;
        }
    }

    rt_kprintf("\nArchive library: valid count=%u, stored count=%u, capacity=%u\n",
               (unsigned int)valid_count,
               (unsigned int)g_inv_archive_lib.count,
               (unsigned int)INVERTER_ARCHIVE_MAX_COUNT);
    rt_kprintf("%-5s %-11s %-15s %-32s %-13s %-18s\n",
               "No.", "Valid", "Modbus address", "Manufacturer",
               "Protocol ver", "Access port");

    for (index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index)
    {
        const Inv_ArchiveSlot_t *slot = &g_inv_archive_lib.slots[index];
        char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U];
        char modbus_address[16];

        inv_archive_copy_mfr_name(manufacturer, slot->archive.mfr_info.name);
        rt_snprintf(modbus_address,
                    sizeof(modbus_address),
                    "0x%02X(%-3u)",
                    (unsigned int)slot->archive.mb_addr,
                    (unsigned int)slot->archive.mb_addr);
        rt_kprintf("%-5u %-7s(%u)  %-15s %-32s 0x%02X%02X        %u(%s)\n",
                   (unsigned int)(index + 1U),
                   slot->valid == INVERTER_ARCHIVE_VALID ? "VALID" : "INVALID",
                   (unsigned int)slot->valid,
                   modbus_address,
                   manufacturer[0] != '\0' ? manufacturer : "(empty)",
                   (unsigned int)slot->archive.mfr_info.proto_ver[0],
                   (unsigned int)slot->archive.mfr_info.proto_ver[1],
                   (unsigned int)slot->archive.port,
                   inv_archive_port_name(slot->archive.port));
    }

    rt_kprintf("Printed %u archive slots, valid count=%u\n",
               (unsigned int)INVERTER_ARCHIVE_MAX_COUNT,
               (unsigned int)valid_count);
}

static int inv_archive_print(void)
{
    Inv_Archive_Print();
    return 0;
}
MSH_CMD_EXPORT(inv_archive_print, print all inverter archive slots);
