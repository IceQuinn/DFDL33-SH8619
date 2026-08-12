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
#include "ctu_cfg.h"
#include "main_uart.h"


/* 线程结构体 */
typedef struct
{
    char        *name;                                  /**< the name of thread */
    void        (*entry)(void *parameter);              /**entry the entry function of thread*/
    void        *parameter;                             /**parameter the parameter of thread enter function*/
    rt_uint32_t stack_size;                             /**stack_size the size of thread stack*/
    rt_uint8_t  priority;                               /**priority the priority of thread*/
    rt_uint32_t tick;                                   /**tick the time slice if there are same priority thread*/
}user_thread_table_typedef;

const user_thread_table_typedef user_thread_table[] = {
    {"uart_mgmt",     uart_mgmt_thread_entry,      RT_NULL,    1024,   17, 15},    /* UART管理线程 */
        // {"measure",     measurement_entry,      RT_NULL,    1024,   17, 15},    /* 计量线程 */
        // {"meas_data",   meas_data_deal,         RT_NULL,    1024,   22, 15},    /* 测量数据线程 */
        // {"record",      Record_Wave_Thread,     RT_NULL,    1024,   6,  15},    /* 录波线程 */
        // {"event",       Event_Deal_Loop,        RT_NULL,    1024,   30, 10},    /* 事件记录线程 */
        // {"btn",         btn_thread_entry,       RT_NULL,    768,    9,  10},    /* 按键线程 */
        // {"ui",          ui_loop,                RT_NULL,    2048,   26, 10},    /* 显示线程 */
        // {"relay",       relay_thread_entry,     RT_NULL,    768,    8,  10},    /* 保护+交流量计算线程 */
        // {"md_send",     send_thread_entry,      RT_NULL,    1280,   29, 10},    /* 转存数据线程 */
        // {"md_s_poll",   modbus_deal_thread,     RT_NULL,    1024,   21, 10},    /* ModBus解析线程 */
        // {"temp",        user_temp_thread,       RT_NULL,    4096,   29, 10},    /* 温度检测线程 */
        // {"led_run",     User_Led_Thread_Entry,  RT_NULL,    512,    25, 10},    /* LED灯运行线程 */
        // {"645_sl",      dlt645_deal_thread_entry, NULL,     2048,   20, 15},    /* 645解析线程 */
};

void user_thread_init(void)
{
    rt_thread_t tid1 = RT_NULL;
    for(uint16_t i=0; i<countof(user_thread_table); i++)
    {
        tid1 = rt_thread_create(user_thread_table[i].name,
                user_thread_table[i].entry,
                user_thread_table[i].parameter,
                user_thread_table[i].stack_size,
                user_thread_table[i].priority,
                user_thread_table[i].tick);
        if (tid1 != RT_NULL)
        {
            rt_thread_startup(tid1);
        }
        else
        {
            LOG_E("%s thread create fail", user_thread_table[i].name);
        }
    }
}

int main(void)
{
    Sys_Run_Time_Init();        //记录上电时间

    show_ctu_msg();             //打印装置信息

    flash_init();               //flash_sfud初始化

    config_para_init();

    uart_init();

    Inv_Proto_Init();             //逆变器协议库初始化

    Inv_Archive_Init();             // 档案校验

    user_thread_init();


    return RT_EOK;
}
