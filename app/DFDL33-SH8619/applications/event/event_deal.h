/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-11     mutou       the first version
 */
#ifndef APPLICATIONS_EVENT_EVENT_DEAL_H_
#define APPLICATIONS_EVENT_EVENT_DEAL_H_

#include "drv_common.h"
#include "event_cfg.h"
#include <time.h>

#define EVENTS_VER              1
#define MAX_EVENT               256


#define ACT_VAL_SIZE 23 // 动作测量值缓冲区大小

// 事件记录提供的几种动作值类型（适用与不同动作值的事件）
enum{
    VAL_TYPE_RELAY = 0, // 保护/告警动作值
    VAL_TYPE_MAX,
};


#pragma pack(1) //一字节对齐

struct act_val_str
{
    uint8_t  phase;     //故障相别, 1:A相电压，2:B相电压，3:C相电压，4:A相电流，5:B相电流，6:C相电流，7:N相电流/剩余电流
    uint16_t act_v_ua;
    uint16_t act_v_ub;
    uint16_t act_v_uc;
    uint32_t act_v_ia;
    uint32_t act_v_ib;
    uint32_t act_v_ic;
    uint32_t act_v_3i0;
};

enum {
    FAULT_PHASE_UA = 1,
    FAULT_PHASE_UB  ,
    FAULT_PHASE_UC  ,

    FAULT_PHASE_IA  ,
    FAULT_PHASE_IB  ,
    FAULT_PHASE_IC  ,
    FAULT_PHASE_IN_R,
};

struct Event_Str // 28字节
{
    uint16_t event_id;      //事件类型
    uint16_t event_type;    //事件序号
    uint32_t times;         //事件触发时间
    uint16_t ms;            //事件触发时间-ms
    uint16_t val;           //动作值
    uint16_t val_type;      // 1动作测量值，2电压波动值
    union{
        uint8_t act_val_buf[ACT_VAL_SIZE]; //动作测量值缓冲区
        struct act_val_str act_val; //动作测量值
    };

    uint32_t duration_ms;   //持续时间
};

// 新规范里Modbus上送的数据格式
typedef struct
{
    uint8_t  event_id;          // 事件代码
    uint8_t  event_type;        // 事件分类
    uint8_t  event_month;       // 事件年月
    uint8_t  event_year;
    uint8_t  event_hour;        // 事件日时
    uint8_t  event_day;
    uint8_t  event_second;      // 事件分秒
    uint8_t  event_minute;
    uint16_t event_ms;          // 事件毫秒

    uint8_t  event_val;         // 事件动作值
    uint8_t  event_alarm_level; // 事件告警等级

    int32_t  event_act_val1;    // 事件动作测量值1
    int32_t  event_act_val2;    // 事件动作测量值2
    int32_t  event_act_val3;    // 事件动作测量值3

    uint16_t reserve1;
    uint16_t reserve2;
    uint16_t reserve3;
    uint16_t reserve4;

}UpEventTypeDef;

#pragma pack() //一字节对齐

void Event_Init(void);
void Event_Deal_Loop(void *parameter);

int get_next_event_idx(int evt_class, int cur_idx);
int get_prev_event_idx(int evt_class, int cur_idx);

uint16_t get_event_num(int evt_class);
int16_t get_relatively_event_idx(int evt_class, int cur_idx);
struct Event_Str* get_event(int evt_class, int event_page_idx);

//int32_t Send_Event_To_Cache(struct Event_Str *p_Event_Date);
int32_t Send_Event_To_Cache(uint16_t event_id, uint16_t val, struct act_val_str *p_act_val);
int32_t Send_Event_To_Cache_By_Type(uint16_t event_id, uint16_t val, void *p_act_val, uint16_t act_type);
void Clear_Events(uint8_t  event_class);


void Clear_All_Events(void);
void Clear_Events_Type(uint16_t  event_class);

#endif /* APPLICATIONS_EVENT_EVENT_DEAL_H_ */
