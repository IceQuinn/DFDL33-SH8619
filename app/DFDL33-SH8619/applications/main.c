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
#include "cycle_loop.h"
#include "time_ctrl.h"
#include "event_deal.h"
#include "led.h"
#include "voltage_acq.h"
#include "dlt645_deal.h"


/* 应用线程创建参数表。 */
typedef struct User_Thread_Table {
    char        *name;                                  /**< the name of thread */
    void        (*entry)(void *parameter);              /**entry the entry function of thread*/
    void        *parameter;                             /**parameter the parameter of thread enter function*/
    uint32_t stack_size;                                /**stack_size the size of thread stack*/
    uint8_t priority;                                   /**priority the priority of thread*/
    uint32_t tick;                                      /**tick the time slice if there are same priority thread*/
} user_thread_table_typedef;

const user_thread_table_typedef user_thread_table[] = {
    {"uart_mgmt",     uart_mgmt_thread_entry,      RT_NULL,    1024,   17, 15},    /* UART管理线程 */
    {"cycle",     cycle_loop_thread_entry,      RT_NULL,    4096,   17, 15},    /* 周期抄读线程 */
    {"time_ctrl",   time_ctrl_thread_entry,         RT_NULL,    2048,   22, 15},    /* 时段控线程 */
        // {"record",      Record_Wave_Thread,     RT_NULL,    1024,   6,  15},    /* 录波线程 */
    {"event",       Event_Deal_Loop,        RT_NULL,    1024,   30, 10},    /* 事件记录线程 */
        // {"btn",         btn_thread_entry,       RT_NULL,    768,    9,  10},    /* 按键线程 */
        // {"ui",          ui_loop,                RT_NULL,    2048,   26, 10},    /* 显示线程 */
        // {"relay",       relay_thread_entry,     RT_NULL,    768,    8,  10},    /* 保护+交流量计算线程 */
        // {"md_send",     send_thread_entry,      RT_NULL,    1280,   29, 10},    /* 转存数据线程 */
        // {"md_s_poll",   modbus_deal_thread,     RT_NULL,    1024,   21, 10},    /* ModBus解析线程 */
        // {"temp",        user_temp_thread,       RT_NULL,    4096,   29, 10},    /* 温度检测线程 */
    {"led_run",     User_Led_Thread_Entry,  RT_NULL,    512,    25, 10},    /* LED灯运行线程 */
       {"645_sl",      dlt645_deal_thread_entry, NULL,     2048,   20, 15},    /* 645解析线程 */
    {"voltage_acq", voltage_acq_thread_entry,  RT_NULL, 1024,   15, 10},
};

/* 按线程参数表依次创建并启动全部应用线程。 */
void user_thread_init(void)
{
    rt_thread_t tid1 = RT_NULL;
    uint16_t index;

    /* 遍历线程表，每次创建并启动一个应用线程。 */
    for(index = 0U; index < countof(user_thread_table); ++index) {
        tid1 = rt_thread_create(user_thread_table[index].name,
                                user_thread_table[index].entry,
                                user_thread_table[index].parameter,
                                user_thread_table[index].stack_size,
                                user_thread_table[index].priority,
                                user_thread_table[index].tick);

        /* 线程对象创建成功时立即启动线程。 */
        if(tid1 != RT_NULL) {
            rt_thread_startup(tid1);
        }
        /* 线程对象创建失败时记录线程名称，后续线程仍继续创建。 */
        else {
            LOG_E("%s thread create fail", user_thread_table[index].name);
        }
    }
}

/* 应用入口按照依赖顺序初始化基础模块、协议档案和业务线程。 */
int main(void)
{
    Sys_Run_Time_Init();        /* 记录上电时间。 */

    show_ctu_msg();             /* 打印装置信息。 */

    flash_init();               /* 初始化外部Flash。 */

    Ctu_Cfg_Init();             /* 装载装置配置。 */

    Dlt645_Init();              /* 初始化645协议库。 */
    
    Inv_Proto_Init();           /* 协议库必须先于档案库初始化。 */

    Inv_Archive_Init();         /* 装载档案并建立运行时协议指针。 */

    Event_Init();               /* 事件初始化 */

    uart_init();                /* 初始化串口后才能启动通信线程。 */

    user_thread_init();         /* 基础资源就绪后创建应用线程。 */
    return RT_EOK;
}
