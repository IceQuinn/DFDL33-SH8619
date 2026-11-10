// /*
// * Copyright (c) 2006-2021, RT-Thread Development Team
// *
// * SPDX-License-Identifier: Apache-2.0
// *
// * Change Logs:
// * Date           Author       Notes
// * 2026-08-12     mutou       the first version
// */
// #include "inv_archive_mgmt.h"

// #include "drv_ex_flash.h"
// #include "user_ex_flash_mgmt.h"

// /* 全局逆变器档案库，固定提供12个档案槽位，上电后由应用程序负责装载和维护。 */
// Inv_ArchiveLib_t g_inv_archive_lib;


// /* 重新统计有效档案数，并将不包含运行时协议指针的档案库保存到Flash A/B区。 */
// void Inv_Archive_Save(void)
// {
//     inv_archive_refresh_count();
//     AB_save(flash_write,
//             ARCH_LIB_ADDR_A,
//             ARCH_LIB_ADDR_B,
//             &g_inv_archive_lib,
//             INVERTER_ARCHIVE_VER,
//             sizeof(g_inv_archive_lib),
//             "ARCHIVE LIB");
// }

// /* 清空档案库和运行时协议指针，并将空档案库保存到Flash。 */
// void Inv_Archive_Default_Init(void)
// {
//     rt_memset(&g_inv_archive_lib, 0, sizeof(g_inv_archive_lib)); /* 清除全部持久化档案数据。 */
//     rt_memset(g_inv_archive_proto, 0, sizeof(g_inv_archive_proto)); /* 清除全部运行时协议指针。 */
//     Inv_Archive_Save();
// }
// MSH_CMD_EXPORT(Inv_Archive_Default_Init, Inv_Archive_Default_Init);

// // 档案初始化
// void Inv_Archive_Init(void)
// {
//    int32_t check_sta;

//    rt_memset(g_inv_archive_proto, 0, sizeof(g_inv_archive_proto)); /* Flash不保存指针，上电时必须重新建立。 */
//    check_sta = AB_check(flash_read,          // 读接口
//                         flash_write,         // 写接口
//                         ARCH_LIB_ADDR_A,     // A区地址
//                         ARCH_LIB_ADDR_B,     // B区地址
//                         &g_inv_archive_lib,  // 数据内存
//                         sizeof(g_inv_archive_lib), // 数据大小
//                         "ARCHIVE LIB");      // 数据描述

//    /* Flash校验失败、版本不一致或数据长度异常时恢复默认空档案库。 */
//    if((check_sta == 1) ||
//       (g_inv_archive_lib.head.ver != INVERTER_ARCHIVE_VER) ||
//       (g_inv_archive_lib.head.len != (sizeof(g_inv_archive_lib) - sizeof(rcd_head)))) {
//        rt_kprintf("[%08d] archive library check failed, loading defaults\n", rt_tick_get());
//        Inv_Archive_Default_Init();
//    }

//    Inv_Archive_Validate_Protocols(); /* 协议库已先初始化，此处立即为全部有效档案建立协议指针。 */
// }

