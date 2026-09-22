/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-11     mutou       the first version
 */
#ifndef APPLICATIONS_EVENT_EVENT_CFG_H_
#define APPLICATIONS_EVENT_EVENT_CFG_H_

#include "drv_common.h"

/* 事件表 */
struct event_desc_map
{
    uint16_t evt_id;          //事件id
    uint16_t evt_class;         //事件分类
    uint16_t save_act_val_flg;  //要不要保存动作值
    uint16_t act_val_num;       //保存几个动作值
    const char* evt_desc;
};

extern struct event_desc_map event_desc[];
extern uint32_t Event_Table_Size;

/* 事件分类 */
enum
{
    EVT_CLASS_SELF_CHECK,   // 自检事项
    EVT_CLASS_OPERATE,      // 操作事项

    EVT_CLASS_MAX,
};

enum
{
    EVT_RESET,    // 事件动作
    EVT_SET,      // 事件复归
};

enum EVENT_TYPE_2
{
    // 操作事件 事件类型3
    ETP_POWER_DOWN      = 30,       //掉电
    ETP_POWER_UP        = 31,       //上电
    ETP_CAL_PARA_REINIT,            //校准参数初始化
    ETP_RES_DEFAULT_PARA,           //参数初始化
    ETP_REBOOT,                     //重启
    ETP_SCAP_ERR,                   //超级电容故障
};


const struct event_desc_map* get_event_desc(uint16_t evt_type);



#endif /* APPLICATIONS_EVENT_EVENT_CFG_H_ */
