/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_archive.h"

#include "drv_ex_flash.h"
#include "inverter_protocol_library.h"
#include "user_ex_flash_mgmt.h"

/* 全局逆变器档案库，固定提供12个档案槽位，上电后由应用程序负责装载和维护。 */
Inv_ArchiveLib_t g_inv_archive_lib;

/* 协议指针只在RAM中使用，不能随档案库写入Flash。 */
const struct Inv_Proto *g_inv_archive_proto[INVERTER_ARCHIVE_MAX_COUNT];

/* 按槽位有效标志重新统计档案数，避免增量修改导致count与槽位状态不一致。 */
static void inv_archive_refresh_count(void)
{
    uint8_t index;
    uint8_t valid_count = 0U;

    /* 遍历固定档案槽位，只统计有效标志为有效的档案。 */
    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index) {
        /* 当前槽位有效时累计档案数量，无效槽位不参与统计。 */
        if(g_inv_archive_lib.valid[index] == INVERTER_ARCHIVE_VALID) {
            ++valid_count;
        }
    }

    g_inv_archive_lib.count = valid_count;
}

/* 按厂家名称和规约版本查找有效协议，没有匹配项时返回RT_NULL。 */
static const Inv_Proto_t *inv_archive_find_protocol(const Inv_MfrInfo_t *mfr_info)
{
    uint16_t protocol_index;

    /* 调用方没有提供厂家信息时无法匹配协议。 */
    if(mfr_info == RT_NULL) {
        return RT_NULL;
    }

    /* 依次检查协议库中的全部协议槽位。 */
    for(protocol_index = 0U; protocol_index < INVERTER_PROTOCOL_LIBRARY_COUNT; ++protocol_index) {
        /* 只有有效协议，并且厂家名称和规约版本完全相同时才算匹配成功。 */
        if((g_inv_proto_lib.valid[protocol_index] == INVERTER_PROTOCOL_VALID) &&
           (rt_memcmp(&g_inv_proto_lib.proto[protocol_index].mfr_info, mfr_info, sizeof(*mfr_info)) == 0)) {
            return &g_inv_proto_lib.proto[protocol_index];
        }
    }

    return RT_NULL;
}

/* 重新统计有效档案数，并将不包含运行时协议指针的档案库保存到Flash A/B区。 */
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

/* 新增或更新档案，并同步建立该档案与协议库之间的运行时指针关系。 */
int8_t Inv_Archive_Add(const Inv_Archive_t *archive)
{
    uint8_t index;
    uint8_t empty_index = INVERTER_ARCHIVE_MAX_COUNT;

    /* 档案为空、Modbus地址越界或端口号越界时拒绝写入。 */
    if((archive == RT_NULL) ||
       (archive->mb_addr < 1U) || (archive->mb_addr > 247U) ||
       (archive->port < INV_PORT_RJ45_1) || (archive->port > INV_PORT_WIRELESS)) {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    /* 遍历档案槽位，优先更新已有设备，同时记录第一个空闲槽位。 */
    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index) {
        Inv_Archive_t *stored_archive = &g_inv_archive_lib.archives[index];

        /* 有效槽位需要判断是否为相同端口、相同地址的同一台设备。 */
        if(g_inv_archive_lib.valid[index] == INVERTER_ARCHIVE_VALID) {
            /* 同一接入端口和Modbus地址代表同一设备，重新识别时更新原档案。 */
            if((stored_archive->port == archive->port) && (stored_archive->mb_addr == archive->mb_addr)) {
                rt_memcpy(stored_archive, archive, sizeof(*stored_archive));
                g_inv_archive_proto[index] = inv_archive_find_protocol(&stored_archive->mfr_info);
                Inv_Archive_Save();
                return (int8_t)index;
            }
        }
        /* 当前槽位无效，并且尚未记录空槽时保存该槽位下标。 */
        else if(empty_index == INVERTER_ARCHIVE_MAX_COUNT) {
            empty_index = index;
        }
    }

    /* 没有找到空闲槽位时表示档案库已满。 */
    if(empty_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    /* 先复制完整档案再置有效标志，避免其他读取者看到只填写了一部分的有效档案。 */
    rt_memcpy(&g_inv_archive_lib.archives[empty_index], archive, sizeof(g_inv_archive_lib.archives[empty_index]));
    g_inv_archive_lib.valid[empty_index] = INVERTER_ARCHIVE_VALID;
    g_inv_archive_proto[empty_index] = inv_archive_find_protocol(&g_inv_archive_lib.archives[empty_index].mfr_info);
    Inv_Archive_Save();
    return (int8_t)empty_index;
}

/* 根据设备地址、接入端口和厂家信息生成档案，再调用统一档案新增接口。 */
int8_t Inv_Archive_Add_Device(uint8_t mb_addr, uint8_t port, const Inv_MfrInfo_t *mfr_info)
{
    Inv_Archive_t archive;

    /* 厂家信息为空时无法生成完整档案。 */
    if(mfr_info == RT_NULL) {
        return INVERTER_ARCHIVE_ADD_FAILED;
    }

    rt_memset(&archive, 0, sizeof(archive));
    archive.mb_addr = mb_addr;
    archive.port = port;
    rt_memcpy(&archive.mfr_info, mfr_info, sizeof(archive.mfr_info)); /* 整体复制厂家名称和规约版本。 */
    return Inv_Archive_Add(&archive);
}

/* 校验全部有效档案，并为能够匹配协议的档案建立运行时协议指针。 */
void Inv_Archive_Validate_Protocols(void)
{
    uint8_t valid_count = 0U;
    uint8_t archive_changed = 0U;

    /* 逐个检查固定档案槽位，避免只按count遍历时遗漏中间的有效槽位。 */
    for(uint8_t i = 0U; i < INVERTER_ARCHIVE_MAX_COUNT; ++i) {
        Inv_Archive_t *archive = &g_inv_archive_lib.archives[i];
        const Inv_Proto_t *protocol;

        /* 无效档案不参与协议匹配，同时清除可能残留的运行时指针。 */
        if(g_inv_archive_lib.valid[i] != INVERTER_ARCHIVE_VALID) {
            g_inv_archive_proto[i] = RT_NULL;
            continue;
        }

        protocol = inv_archive_find_protocol(&archive->mfr_info);

        /* 有效档案找不到协议时，将该档案置为无效并清除协议指针。 */
        if(protocol == RT_NULL) {
            rt_kprintf("[%08d] archive slot[%d] addr[%d] port[%d] invalid, protocol not found\n", rt_tick_get(), i + 1, archive->mb_addr, archive->port);
            g_inv_archive_lib.valid[i] = INVERTER_ARCHIVE_INVALID;
            g_inv_archive_proto[i] = RT_NULL;
            archive_changed = 1U;
            continue;
        }

        g_inv_archive_proto[i] = protocol; /* 保存协议对象地址，周期抄读时无需再次遍历协议库。 */
        ++valid_count;
    }

    /* 历史count和本次有效档案统计不一致时，同步修正持久化计数。 */
    if(g_inv_archive_lib.count != valid_count) {
        g_inv_archive_lib.count = valid_count;
        archive_changed = 1U;
    }

    /* 只有档案有效状态或档案数量发生变化时才写Flash，减少擦写次数。 */
    if(archive_changed != 0U) {
        Inv_Archive_Save();
    }
}

/* 按档案槽位获取已匹配的协议指针，无效档案、未匹配档案或越界均返回RT_NULL。 */
const struct Inv_Proto *Inv_Archive_Get_Protocol(uint8_t archive_index)
{
    /* 下标越界或档案无效时禁止返回协议指针。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID)) {
        return RT_NULL;
    }

    return g_inv_archive_proto[archive_index];
}

/* 查询指定端口是否已被任意一个有效档案占用。 */
uint8_t Inv_Archive_Port_Is_Occupied(uint8_t port)
{
    uint8_t archive_index;

    /* 遍历全部固定槽位，查找端口号相同的有效档案。 */
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        const Inv_Archive_t *archive = &g_inv_archive_lib.archives[archive_index];

        /* 档案有效并且端口号相同时，说明该端口已经被占用。 */
        if((g_inv_archive_lib.valid[archive_index] == INVERTER_ARCHIVE_VALID) && (archive->port == port)) {
            return 1U;
        }
    }

    return 0U;
}

/* 清空档案库和运行时协议指针，并将空档案库保存到Flash。 */
void Inv_Archive_Default_Init(void)
{
    rt_memset(&g_inv_archive_lib, 0, sizeof(g_inv_archive_lib)); /* 清除全部持久化档案数据。 */
    rt_memset(g_inv_archive_proto, 0, sizeof(g_inv_archive_proto)); /* 清除全部运行时协议指针。 */
    Inv_Archive_Save();
}
MSH_CMD_EXPORT(Inv_Archive_Default_Init, Inv_Archive_Default_Init);

/* 从Flash装载档案库，装载失败时恢复空档案库，并为有效档案重新关联协议。 */
void Inv_Archive_Init(void)
{
    int32_t check_sta;

    rt_memset(g_inv_archive_proto, 0, sizeof(g_inv_archive_proto)); /* Flash不保存指针，上电时必须重新建立。 */
    check_sta = AB_check(flash_read,          // 读接口
                         flash_write,         // 写接口
                         ARCH_LIB_ADDR_A,     // A区地址
                         ARCH_LIB_ADDR_B,     // B区地址
                         &g_inv_archive_lib,  // 数据内存
                         sizeof(g_inv_archive_lib), // 数据大小
                         "ARCHIVE LIB");      // 数据描述

    /* Flash校验失败、版本不一致或数据长度异常时恢复默认空档案库。 */
    if((check_sta == 1) ||
       (g_inv_archive_lib.head.ver != INVERTER_ARCHIVE_LIBRARY_VERSION) ||
       (g_inv_archive_lib.head.len != (sizeof(g_inv_archive_lib) - sizeof(rcd_head)))) {
        rt_kprintf("[%08d] 逆变器档案库出错\n", rt_tick_get());
        Inv_Archive_Default_Init();
    }

    Inv_Archive_Validate_Protocols(); /* 协议库已先初始化，此处立即为全部有效档案建立协议指针。 */
}
