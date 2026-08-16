/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_archive.h"

#include "drv_ex_flash.h"
#include "inverter_protocol_library.h"
#include "user_ex_flash_mgmt.h"

/* 全局逆变器档案库，固定提供12个档案槽位，上电后由应用程序负责装载和维护。 */
Inv_ArchiveLib_t g_inv_archive_lib;

/* 按槽位有效标志重新统计档案数，避免增量修改导致count与槽位状态不一致。 */
static void inv_archive_refresh_count(void)
{
    uint8_t index;
    uint8_t valid_count = 0U;

    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index)
    {
        if(g_inv_archive_lib.slots[index].valid == INVERTER_ARCHIVE_VALID)
        {
            ++valid_count;
        }
    }

    g_inv_archive_lib.count = valid_count;
}

/* 按厂家名称和规约版本查找有效协议，返回值等于协议库容量表示没有匹配项。 */
static uint16_t inv_archive_find_protocol(const Inv_MfrInfo_t *mfr_info)
{
    uint16_t protocol_index;

    if(mfr_info == RT_NULL)
    {
        return INVERTER_PROTOCOL_LIBRARY_COUNT;
    }

    for(protocol_index = 0U;
        protocol_index < INVERTER_PROTOCOL_LIBRARY_COUNT;
        ++protocol_index)
    {
        if((g_inv_proto_lib.valid[protocol_index] == INVERTER_PROTOCOL_VALID) &&
           (rt_memcmp(&g_inv_proto_lib.proto[protocol_index].mfr_info,
                      mfr_info,
                      sizeof(*mfr_info)) == 0))
        {
            return protocol_index;
        }
    }

    return INVERTER_PROTOCOL_LIBRARY_COUNT;
}

/* 重新统计count后写入Flash A/B区，AB_save会同步更新版本、长度和CRC。 */
void Inv_Archive_Save(void)
{
    inv_archive_refresh_count();
    AB_save(flash_write,
            ARCH_LIB_ADDR_A,
            ARCH_LIB_ADDR_B,
            &g_inv_archive_lib,
            INVERTER_ARCHIVE_LIBRARY_VERSION,
            sizeof(g_inv_archive_lib),
            "ARCHIVE LIB");
}

int8_t Inv_Archive_Add(const Inv_Archive_t *archive)
{
    uint8_t index;
    uint8_t empty_index = INVERTER_ARCHIVE_MAX_COUNT;

    if((archive == RT_NULL) ||
       (archive->mb_addr < 1U) || (archive->mb_addr > 247U) ||
       (archive->port < INV_PORT_RJ45_1) || (archive->port > INV_PORT_WIRELESS))
    {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index)
    {
        Inv_ArchiveSlot_t *slot = &g_inv_archive_lib.slots[index];

        if(slot->valid == INVERTER_ARCHIVE_VALID)
        {
            /* 同一接入端口和Modbus地址代表同一设备，重新识别时更新而不是重复新增。 */
            if((slot->archive.port == archive->port) &&
               (slot->archive.mb_addr == archive->mb_addr))
            {
                rt_memcpy(&slot->archive, archive, sizeof(slot->archive));
                Inv_Archive_Save();
                return (int8_t)index;
            }
        }
        else if(empty_index == INVERTER_ARCHIVE_MAX_COUNT)
        {
            empty_index = index;        /* 只记录第一个空槽，保持档案编号尽量连续。 */
        }
    }

    if(empty_index >= INVERTER_ARCHIVE_MAX_COUNT)
    {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    /* 先复制完整档案再置有效标志，避免其他读取者看到只填写了一部分的有效档案。 */
    rt_memcpy(&g_inv_archive_lib.slots[empty_index].archive,
              archive,
              sizeof(g_inv_archive_lib.slots[empty_index].archive));
    g_inv_archive_lib.slots[empty_index].valid = INVERTER_ARCHIVE_VALID;
    Inv_Archive_Save();
    return (int8_t)empty_index;
}

int8_t Inv_Archive_Add_Device(uint8_t mb_addr,
                              uint8_t port,
                              const Inv_MfrInfo_t *mfr_info)
{
    Inv_Archive_t archive;

    if(mfr_info == RT_NULL)
    {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    rt_memset(&archive, 0, sizeof(archive));
    archive.mb_addr = mb_addr;
    archive.port = port;
    /* 厂家名称和规约版本共用Inv_MfrInfo_t，整体复制可保持固定长度字段不变。 */
    rt_memcpy(&archive.mfr_info, mfr_info, sizeof(archive.mfr_info));
    return Inv_Archive_Add(&archive);
}

void Inv_Archive_Validate_Protocols(void)
{
    uint8_t archive_index;
    uint8_t valid_count = 0U;
    uint8_t archive_changed = 0U;

    for(archive_index = 0U;
        archive_index < INVERTER_ARCHIVE_MAX_COUNT;
        ++archive_index)
    {
        Inv_ArchiveSlot_t *slot = &g_inv_archive_lib.slots[archive_index];

        if(slot->valid != INVERTER_ARCHIVE_VALID)
        {
            continue;
        }

        if(inv_archive_find_protocol(&slot->archive.mfr_info) >=
           INVERTER_PROTOCOL_LIBRARY_COUNT)
        {
            rt_kprintf("[%08d] archive slot[%u] addr[%u] port[%u] invalid, protocol not found\n",
                       (int)rt_tick_get(),
                       (unsigned int)(archive_index + 1U),
                       (unsigned int)slot->archive.mb_addr,
                       (unsigned int)slot->archive.port);
            slot->valid = INVERTER_ARCHIVE_INVALID;
            archive_changed = 1U;
            continue;
        }

        ++valid_count;
    }

    /* count也参与后续流程判断，发现历史计数不一致时同步修正并保存。 */
    if(g_inv_archive_lib.count != valid_count)
    {
        g_inv_archive_lib.count = valid_count;
        archive_changed = 1U;
    }

    if(archive_changed != 0U)
    {
        Inv_Archive_Save();
    }
}

uint8_t Inv_Archive_Port_Is_Occupied(uint8_t port)
{
    uint8_t archive_index;

    for(archive_index = 0U;
        archive_index < INVERTER_ARCHIVE_MAX_COUNT;
        ++archive_index)
    {
        const Inv_ArchiveSlot_t *slot = &g_inv_archive_lib.slots[archive_index];

        if((slot->valid == INVERTER_ARCHIVE_VALID) &&
           (slot->archive.port == port))
        {
            return 1U;
        }
    }

    return 0U;
}


void Inv_Archive_Default_Init(void)
{
    /* 先清空整个库，确保没有默认协议的槽位保持无效且不存在Flash残留数据。 */
    rt_memset(&g_inv_archive_lib, 0, sizeof(g_inv_archive_lib));

    Inv_Archive_Save();
}
MSH_CMD_EXPORT(Inv_Archive_Default_Init, Inv_Archive_Default_Init);


void Inv_Archive_Init(void)
{
    int check_sta = 0;
    check_sta = AB_check(flash_read,        //读接口
                    flash_write,            //写接口
                    ARCH_LIB_ADDR_A,       //A区地址
                    ARCH_LIB_ADDR_B,       //B区地址
                    &g_inv_archive_lib,       //数据内存
                    sizeof(g_inv_archive_lib),//数据大小
                    "ARCHIVE LIB");           //描述
    if((1 == check_sta) ||
       (g_inv_archive_lib.head.ver != INVERTER_ARCHIVE_LIBRARY_VERSION) ||
       (g_inv_archive_lib.head.len !=
        (sizeof(g_inv_archive_lib) - sizeof(rcd_head))))
    {
        rt_kprintf("逆变器档案库出错\n");
        Inv_Archive_Default_Init();
    }
}
