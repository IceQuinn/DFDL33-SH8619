/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#ifndef APPLICATIONS_INVERTER_ARCHIVE_H
#define APPLICATIONS_INVERTER_ARCHIVE_H

#include <stdint.h>

#include "AB_check.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVERTER_ARCHIVE_MAX_COUNT          12U
#define INVERTER_ARCHIVE_BRAND_WIRE_SIZE    32U
#define INVERTER_ARCHIVE_PROTOCOL_VERSION_SIZE  2U
#define INVERTER_ARCHIVE_WIRE_SIZE          36U

#define INVERTER_ARCHIVE_ADDRESS_UNUSED     0xFFU
#define INVERTER_ARCHIVE_PROTOCOL_UNKNOWN_BYTE  0xFFU

#define INVERTER_ARCHIVE_LIBRARY_VERSION           1U   // 档案库版本号
#define INVERTER_ARCHIVE_INVALID                   0U   // 无效档案
#define INVERTER_ARCHIVE_VALID                     1U   // 有效档案


/* 共享厂家信息。档案和协议库必须直接使用本结构体，禁止各自重复定义厂家 名称及规约版本字段。结构体按1字节对齐，内容与档案线上34字节厂家信息 完全一致：厂家ASCII名称32字节 + 规约版本2字节。 */
#pragma pack(1)
typedef struct Inv_MfrInfo
{
    /* 固定32字节ASCII名称；占满时不保证包含字符串结束符。 */
    char name[INVERTER_ARCHIVE_BRAND_WIRE_SIZE];

    /* 规约版本原始2字节，保持上行协议规定的传输顺序。 */
    uint8_t proto_ver[INVERTER_ARCHIVE_PROTOCOL_VERSION_SIZE];
} Inv_MfrInfo_t;


#define INV_MFR_INFO_SIZE                   34U
typedef char Inv_MfrInfoSizeCheck_t[
    (sizeof(Inv_MfrInfo_t) == INV_MFR_INFO_SIZE) ? 1 : -1];

/* 逆变器接入端口。 */
typedef enum Inv_Port
{
    INV_PORT_RJ45_1 = 1,  /* 端口1：RJ45-I。 */
    INV_PORT_RJ45_2 = 2,  /* 端口2：RJ45-II。 */
    INV_PORT_RS485_2 = 3, /* 端口3：RS485-II。 */
    INV_PORT_WIRELESS = 4 /* 端口4：无线。 */
} Inv_Port_t;

/* DL/T 645上行协议中的单条光伏逆变器档案。
 * 字段顺序与截图中的档案数据顺序一致：Modbus地址、厂家名称、规约版本、
 * 接入端口。厂家字段直接使用共享的 Inv_MfrInfo_t。按1字节对齐后，结构体
 * 内存布局与36字节线上档案完全一致，但协议编解码仍使用明确的字段偏移。 */
typedef struct Inv_Archive
{
    /* 逆变器Modbus从站地址，正常读取地址范围为1~247；0xFF表示未接入。 */
    uint8_t mb_addr;

    /* 厂家名称及规约版本。 */
    Inv_MfrInfo_t mfr_info;

    /* 接入端口，取值范围1~4，具体含义见 Inv_Port_t。 */
    uint8_t port;
} Inv_Archive_t;

#define INV_ARCHIVE_SIZE                    36U
typedef char Inv_ArchiveSizeCheck_t[
    (sizeof(Inv_Archive_t) == INV_ARCHIVE_SIZE) ? 1 : -1];

/* 单个档案库槽位：valid为1表示archive有效，valid为0表示空槽。 */
typedef struct Inv_ArchiveSlot
{
    uint8_t valid;
    Inv_Archive_t archive;
} Inv_ArchiveSlot_t;


/* 逆变器档案库，固定提供12个槽位，槽位下标0~11对应档案编号1~12。 */
typedef struct Inv_ArchiveLib
{
    /* AB区校验头，仅描述整个档案库，不属于某一条厂家协议。 */
    rcd_head head;

    /* 当前有效档案数量，范围0~12。 */
    uint8_t count;

    /* 固定档案槽位，不因删除中间档案而移动其他槽位。 */
    Inv_ArchiveSlot_t slots[INVERTER_ARCHIVE_MAX_COUNT];
} Inv_ArchiveLib_t;
#pragma pack()

#define INV_ARCHIVE_SLOT_SIZE               37U
#define INV_ARCHIVE_LIB_SIZE                451U
typedef char Inv_ArchiveSlotSizeCheck_t[
    (sizeof(Inv_ArchiveSlot_t) == INV_ARCHIVE_SLOT_SIZE) ? 1 : -1];
typedef char Inv_ArchiveLibSizeCheck_t[
    (sizeof(Inv_ArchiveLib_t) == INV_ARCHIVE_LIB_SIZE) ? 1 : -1];

/* 全局逆变器档案库变量，应用程序可直接按照固定槽位读取或填写档案。 */
extern Inv_ArchiveLib_t g_inv_archive_lib;

void Inv_Archive_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_INVERTER_ARCHIVE_H */
