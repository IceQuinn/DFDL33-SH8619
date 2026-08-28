/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_protocol_library.h"
#include "drv_ex_flash.h"
#include "user_ex_flash_mgmt.h"




Inv_ProtoLib_t g_inv_proto_lib = {0};               /* 保存有效标志和最多100条厂家协议配置。 */
/* 清空协议库，装载内置默认协议，并保存到Flash A/B区。 */
void Inv_Proto_Default_Init(void)
{
    inv_proto_default_lib_init(); /* 按厂家编号从各分项配置表重新组装默认协议库。 */

    AB_save(flash_write, /* 将重建后的默认协议库同时保存到Flash A/B区。 */
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
    int32_t check_sta; /* Flash A/B区协议库校验接口返回状态。 */

    check_sta = AB_check(flash_read,           // 读取Flash的底层接口
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
        Inv_Proto_Default_Init(); /* 持久化数据不可用时按分项配置表重建默认协议。 */
    }
}
