/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-30     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "sys.h"
#include "drv_ex_flash.h"
#include "inverter_protocol_library.h"

int main(void)
{
    Sys_Run_Time_Init();        //记录上电时间

    show_ctu_msg();             //打印装置信息

    flash_init();               //flash_sfud初始化

    Inv_Proto_Init();             //逆变器协议库初始化


    return RT_EOK;
}
