/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_protocol_library.h"
#include "drv_ex_flash.h"
#include "user_ex_flash_mgmt.h"




Inv_ProtoLib_t g_inv_proto_lib = {0};
extern Inv_Proto_t g_inv_proto_default_lib[4];

/* 清空协议库，装载内置默认协议，并保存到Flash A/B区。 */
void Inv_Proto_Default_Init(void)
{
    uint8_t index;

    /* 先清空整个库，确保没有默认协议的槽位保持无效且不存在Flash残留数据。 */
    rt_memset(&g_inv_proto_lib, 0, sizeof(g_inv_proto_lib));

    /* 将全部内置默认协议依次复制到协议库前部并置为有效。 */
    for(index = 0; index < INVERTER_PROTOCOL_DEFAULT_COUNT; ++index) {
        rt_memcpy(&g_inv_proto_lib.proto[index], &g_inv_proto_default_lib[index], sizeof(Inv_Proto_t));
        g_inv_proto_lib.valid[index] = INVERTER_PROTOCOL_VALID;
    }

    AB_save(flash_write,
            PROTO_LIB_ADDR_A,
            PROTO_LIB_ADDR_B,
            &g_inv_proto_lib,
            INVERTER_PROTOCOL_LIBRARY_VERSION,
            sizeof(g_inv_proto_lib),
            "PROTO LIB");
}
MSH_CMD_EXPORT(Inv_Proto_Default_Init, Inv_Proto_Default_Init);

/* 从Flash装载协议库，校验失败、版本异常或长度异常时恢复默认协议库。 */
void Inv_Proto_Init(void)
{
    int32_t check_sta;

    check_sta = AB_check(flash_read,           // 读接口
                         flash_write,          // 写接口
                         PROTO_LIB_ADDR_A,      // A区地址
                         PROTO_LIB_ADDR_B,      // B区地址
                         &g_inv_proto_lib,      // 数据内存
                         sizeof(g_inv_proto_lib), // 数据大小
                         "PROTO LIB");         // 数据描述

    /* Flash校验失败、协议库版本不一致或数据长度异常时恢复内置默认协议。 */
    if((check_sta == 1) ||
       (g_inv_proto_lib.head.ver != INVERTER_PROTOCOL_LIBRARY_VERSION) ||
       (g_inv_proto_lib.head.len != (sizeof(g_inv_proto_lib) - sizeof(rcd_head)))) {
        rt_kprintf("[%08d] protocol library check failed, loading defaults\n", rt_tick_get());
        Inv_Proto_Default_Init();
    }
}
