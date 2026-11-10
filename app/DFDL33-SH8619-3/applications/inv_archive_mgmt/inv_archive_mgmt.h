// /*
// * Copyright (c) 2006-2021, RT-Thread Development Team
// *
// * SPDX-License-Identifier: Apache-2.0
// *
// * Change Logs:
// * Date           Author       Notes
// * 2026-08-12     mutou       the first version
// */
// #ifndef APPLICATIONS_INV_ARCHIVE_MGMT_INV_ARCHIVE_MGMT_H_
// #define APPLICATIONS_INV_ARCHIVE_MGMT_INV_ARCHIVE_MGMT_H_

// #include <stdint.h>

// #include "AB_check.h"

// #define INVERTER_ARCHIVE_MAX_COUNT          12  // 逆变器档案最大数
// #define INVERTER_ARCHIVE_ASCII_SIZE         32  // 逆变器厂家最大字符数

// #define INVERTER_ARCHIVE_VER                2   // 档案库版本号

// /* 共享厂家信息。档案和协议库必须直接使用本结构体，禁止各自重复定义厂家 名称及规约版本字段。结构体按1字节对齐，内容与档案线上34字节厂家信息 完全一致：厂家ASCII名称32字节 + 规约版本2字节。 */
// #pragma pack(1)
// typedef struct Inv_MfrInfo
// {
//    /* 固定32字节ASCII名称；占满时不保证包含字符串结束符。 */
//    char name[INVERTER_ARCHIVE_BRAND_WIRE_SIZE];

//    /* 规约版本。 */
//    uint16_t proto_ver;
// } Inv_MfrInfo_t;


// #define INV_MFR_INFO_SIZE                   34
// typedef char Inv_MfrInfoSizeCheck_t[
//    (sizeof(Inv_MfrInfo_t) == INV_MFR_INFO_SIZE) ? 1 : -1];

// /* 逆变器接入端口。 */
// typedef enum Inv_Port
// {
//    INV_PORT_RJ45_1 = 1,  /* 端口1：RJ45-I。 */
//    INV_PORT_RJ45_2 = 2,  /* 端口2：RJ45-II。 */
//    INV_PORT_RS485_2 = 3, /* 端口3：RS485-II。 */
//    INV_PORT_WIRELESS = 4 /* 端口4：无线。 */
// } Inv_Port_t;

// /* DL/T 645上行协议中的单条光伏逆变器档案。
// * 字段顺序与截图中的档案数据顺序一致：Modbus地址、厂家名称、规约版本、
// * 接入端口。厂家字段直接使用共享的 Inv_MfrInfo_t。按1字节对齐后，结构体
// * 内存布局与36字节线上档案完全一致，但协议编解码仍使用明确的字段偏移。 */
// typedef struct Inv_Archive
// {
//    /* 逆变器Modbus从站地址，正常读取地址范围为1~247；0xFF表示未接入。 */
//    uint8_t mb_addr;

//    /* 厂家名称及规约版本。 */
//    Inv_MfrInfo_t mfr_info;

//    /* 接入端口，取值范围1~4，具体含义见 Inv_Port_t。 */
//    uint8_t port;
// } Inv_Archive_t;

// #define INV_ARCHIVE_SIZE                    36
// typedef char Inv_ArchiveSizeCheck_t[
//    (sizeof(Inv_Archive_t) == INV_ARCHIVE_SIZE) ? 1 : -1];

// /* 逆变器档案库，固定提供12个槽位，槽位下标0~11对应档案编号1~12。 */
// typedef struct Inv_ArchiveLib
// {
//    /* AB区校验头，仅描述整个档案库，不属于某一条厂家协议。 */
//    rcd_head head;

//    /* 当前有效档案数量，范围0~12。 */
//    uint8_t count;

//    /* 每条档案对应一个有效标志：1表示有效，0表示空槽或无效。 */
//    uint8_t valid[INVERTER_ARCHIVE_MAX_COUNT];

//    /* 固定档案数组，不因删除中间档案而移动其他档案。 */
//    Inv_Archive_t archives[INVERTER_ARCHIVE_MAX_COUNT];
// } Inv_ArchiveLib_t;
// #pragma pack()

// #define INV_ARCHIVE_LIB_SIZE                451
// typedef char Inv_ArchiveLibSizeCheck_t[
//    (sizeof(Inv_ArchiveLib_t) == INV_ARCHIVE_LIB_SIZE) ? 1 : -1];

// /* 同一下标的valid[index]与archives[index]共同描述一个固定档案槽位。 */
// extern Inv_ArchiveLib_t g_inv_archive_lib;

// /* 运行时协议指针表与档案槽位一一对应，指针不写入Flash。 */
// extern const struct Inv_Proto *g_inv_archive_proto[INVERTER_ARCHIVE_MAX_COUNT];

// #endif /* APPLICATIONS_INV_ARCHIVE_MGMT_INV_ARCHIVE_MGMT_H_ */
