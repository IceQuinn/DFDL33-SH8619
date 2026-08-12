/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_archive.h"

#include "drv_ex_flash.h"
#include "user_ex_flash_mgmt.h"

/* 全局逆变器档案库，固定提供12个档案槽位，上电后由应用程序负责装载和维护。 */
Inv_ArchiveLib_t g_inv_archive_lib;


void Inv_Archive_Default_Init(void)
{
    /* 先清空整个库，确保没有默认协议的槽位保持无效且不存在Flash残留数据。 */
    rt_memset(&g_inv_archive_lib, 0, sizeof(g_inv_archive_lib));

    AB_save(flash_write,
            ARCH_LIB_ADDR_A,
            ARCH_LIB_ADDR_B,
            &g_inv_archive_lib,
            INVERTER_ARCHIVE_LIBRARY_VERSION,
            sizeof(g_inv_archive_lib),
            "ARCHIVE LIB");
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
