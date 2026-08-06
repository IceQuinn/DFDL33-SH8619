/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_protocol_library.h"
#include "drv_ex_flash.h"
#include "user_ex_flash_mgmt.h"


Inv_Proto_t g_inv_proto_lib[INVERTER_PROTOCOL_LIBRARY_COUNT] = {0};
extern Inv_Proto_t g_inv_proto_default_lib[4];

void Inv_Proto_Default_Init(void)
{
    for(int i=0; i<4; i++)
    {
        rt_memcpy(&g_inv_proto_lib[i], &g_inv_proto_default_lib[i], sizeof(Inv_Proto_t));
    }

    AB_save(flash_write, PROTO_LIB_ADDR_A, PROTO_LIB_ADDR_B, g_inv_proto_lib, 1, sizeof(g_inv_proto_lib), "PROTO LIB");
}


void Inv_Proto_Init(void)
{
    int check_sta = 0;
    check_sta = AB_check(flash_read,        //读接口
                    flash_write,            //写接口
                    PROTO_LIB_ADDR_A,       //A区地址
                    PROTO_LIB_ADDR_B,       //B区地址
                    g_inv_proto_lib,        //数据内存
                    sizeof(g_inv_proto_lib),//数据大小
                    "PROTO LIB");           //描述
    if(1 == check_sta)
    {
        Inv_Proto_Default_Init();
    }
}
