/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-11     mutou       the first version
 */
#include "event_deal.h"

#include "user_rtc.h"
#include "user_ex_flash_mgmt.h"
#include "drv_ex_flash.h"
#include "AB_check.h"
#include <time.h>
#include "sys.h"
#include <string.h>

#define DBG_TAG "event_deal"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

struct ACT_VAL_Table_Str{
    uint32_t Act_Val_Byte;          // 动作字节数
    void (*Show_CB)(void *p_Act);   // 打印接口
};


struct ACT_VAL_Table_Str ACT_VAL_Table[VAL_TYPE_MAX] = {
    {
        sizeof(struct act_val_str) / sizeof(uint8_t),
    },
//    {
//        sizeof(struct vol_fluct_str) / sizeof(uint8_t),
//    },
//    {
//        sizeof(struct peak_sag_int_str) / sizeof(uint8_t),
//    },
//    {
//        sizeof(struct percentage_val_str) / sizeof(uint8_t),
//    },
//    {
//        sizeof(struct hv_str) / sizeof(uint8_t),
//    },
};

#define EVENTS_CACHE_NUM_MAX        20


#pragma pack(1) //一字节对齐
// 事件摘要
struct EventDetail
{
    rcd_head hdr;
    uint16_t eventCnt;  // flash中一共有多少条记录
    uint16_t eventSize; // 单条记录的大小
    uint16_t headIdx;   // flash记录中，第一条记录的启动偏移
    uint16_t tailIdx;   // flash记录中，最后一条记录的启动偏移
    uint32_t crc32;     // 记录区的CRC32值
};

// 事件结构体
struct Events_Str
{
    struct EventDetail EvtDetail;
    struct Event_Str   Event[MAX_EVENT];
};

struct Event_Manger_Str
{
    uint32_t Event_Flash_Addr_A;    // 事件A地址
    uint32_t Event_Flash_Addr_B;    // 事件B地址
    struct   Events_Str Events;     // 事件结构
    char     *Event_Name;           // 事件描述
};

#pragma pack()


struct Event_Manger_Str Event_Manger[EVT_CLASS_MAX] =
{
    {.Event_Flash_Addr_A = FLASH_SELF_CHECK_EVENT_A_ADDR, .Event_Flash_Addr_B = FLASH_SELF_CHECK_EVENT_B_ADDR, .Event_Name = "Flash Event Self Check "},
    {.Event_Flash_Addr_A = FLASH_OPERATE_EVENT_A_ADDR,    .Event_Flash_Addr_B = FLASH_OPERATE_EVENT_B_ADDR,    .Event_Name = "Flash Event Operate    "},
};



struct Event_Str Event_Cache_Date[EVENTS_CACHE_NUM_MAX];// 事件
/* 消息队列控制块 */
static struct rt_messagequeue Event_mq;


int32_t Event_Cache_Init(void)
{
    int32_t result;

    /* 初始化消息队列 */
    result = rt_mq_init(&Event_mq,
                        "Event_mq",
                        Event_Cache_Date,               /* 内存池指向 msg_pool */
                        sizeof(Event_Cache_Date[0]),    /* 每个消息的大小是 1 字节 */
                        sizeof(Event_Cache_Date),       /* 内存池的大小是 msg_pool 的大小 */
                        RT_IPC_FLAG_PRIO);              /* 如果有多个线程等待，优先级大小的方法分配消息 */

    if (result != RT_EOK)
    {
        rt_kprintf("init Event message queue failed.\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

int32_t Send_Event_To_Cache(uint16_t event_id, uint16_t val, struct act_val_str *p_act_val)
{
    struct Event_Str Event_Date = {
            .event_id = event_id,
            .times = time(NULL),
            .ms = get_rtc_ms(),
            .val = val,
            .val_type = VAL_TYPE_RELAY,
            .act_val = (NULL==p_act_val) ? (struct act_val_str){0} : (*p_act_val),
    };

    if(RT_EOK != rt_mq_send(&Event_mq, &Event_Date, sizeof(Event_Date)))
    {
        rt_kprintf("Event Send Error\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

int32_t Send_Event_To_Cache_By_Type(uint16_t event_id, uint16_t val, void *p_act_val, uint16_t act_type)
{
    struct Event_Str Event_Date = {
            .event_id = event_id,
            .times = time(NULL),
            .ms = get_rtc_ms(),
            .val = val,
            .val_type = act_type,
//            .act_val = (NULL==p_act_val) ? (struct act_val_str){0} : (*p_act_val),
    };

    rt_memcpy(Event_Date.act_val_buf, p_act_val, ACT_VAL_Table[act_type].Act_Val_Byte); // 将p_act_val的内容复制到

    if(RT_EOK != rt_mq_send(&Event_mq, &Event_Date, sizeof(Event_Date)))
    {
        rt_kprintf("Event Send Error\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* 根据事件类型保存事件到Flash */
void Save_Events_To_Flash(uint8_t  event_class)
{
    AB_save(flash_write,
            Event_Manger[event_class].Event_Flash_Addr_A,
            Event_Manger[event_class].Event_Flash_Addr_B,
            &Event_Manger[event_class].Events,
            EVENTS_VER,
            sizeof(Event_Manger[event_class].Events),
            Event_Manger[event_class].Event_Name);
}

/* 删除事件 */
int32_t Clear_Event(uint8_t  event_class)
{
    rt_memset(&Event_Manger[event_class].Events, 0, sizeof(Event_Manger[event_class].Events));
    rt_kprintf("%s has been Clear\n", Event_Manger[event_class].Event_Name);
    Save_Events_To_Flash(event_class);
    return RT_EOK;
}

void Clear_Events(uint8_t  event_class)
{
    if(EVT_CLASS_MAX == event_class){
        for (uint16_t i = 0; i < EVT_CLASS_MAX; ++i)
        {
            Clear_Event(i);
        }
    }
    else if(event_class < EVT_CLASS_MAX){
        Clear_Event(event_class);
    }
}

void Event_Cmd_Clear(int argc, void** argv)
{
    if(1 == argc){
        Clear_Events(EVT_CLASS_MAX);
    }
    else {
        uint32_t event_type = atoi(argv[1]);
        Clear_Events(event_type);
    }
}
MSH_CMD_EXPORT(Event_Cmd_Clear, Event_Cmd_Clear);

/* 事件初始化 */
void Event_Init(void)
{
    int32_t check_sta = 0;
    Event_Cache_Init();

    for (uint16_t i = 0; i < EVT_CLASS_MAX; ++i)
    {
        check_sta = AB_check(flash_read,
                            flash_write,
                            Event_Manger[i].Event_Flash_Addr_A,
                            Event_Manger[i].Event_Flash_Addr_B,
                            &Event_Manger[i].Events,
                            sizeof(Event_Manger[i].Events),
                            Event_Manger[i].Event_Name);

        if(1 == check_sta)
        {
            Clear_Event(i);
        }
    }
    Send_Event_To_Cache(ETP_POWER_UP, EVT_SET, RT_NULL);
}

void show_evt_act_val(uint16_t val_type, void *p_Act)
{
    if(VAL_TYPE_RELAY == val_type){
        struct act_val_str *p_relay_act = p_Act;
        LOG_I("phase:%d", p_relay_act->phase);
        LOG_I("ua   :%-4d, ub   :%-4d, uc   :%-4d", p_relay_act->act_v_ua, p_relay_act->act_v_ub, p_relay_act->act_v_uc);
        LOG_I("ia   :%-4d, ib   :%-4d, ic   :%-4d, 3i0  :%-4d", p_relay_act->act_v_ia, p_relay_act->act_v_ib, p_relay_act->act_v_ic, p_relay_act->act_v_3i0);
    }
//    else if (VAL_TYPE_FLUCT == val_type) {
//        struct vol_fluct_str *p_fluct_act = p_Act;
//        LOG_I("Hp_RMS_Max_Ux    :%d, Hp_RMS_Min_Ux      :%d", p_fluct_act->Hp_RMS_Max_Ux, p_fluct_act->Hp_RMS_Min_Ux);
//        LOG_I("Hp_RMS_Diff      :%d, Hp_RMS_Relative    :%d", p_fluct_act->Hp_RMS_Diff, p_fluct_act->Hp_RMS_Relative);
//    }
//    else if (VAL_TYPE_PEAK_SAG_INT == val_type) {
//        struct peak_sag_int_str *p_peak_sag_int = p_Act;
//        LOG_I("Phase    :%d, Vp    :%d, Cnt_Vp    :%d", p_peak_sag_int->phase, p_peak_sag_int->Vp, p_peak_sag_int->Cnt_Vp);
//    }
}

/* 打印单事件 */
int32_t Show_Event(struct Event_Str *p_Event_Date, uint16_t idx, uint8_t simple_en)
{
    struct tm t = {0};

    localtime_r((const time_t*)&p_Event_Date->times, &t);
    for(uint16_t i=0; i<Event_Table_Size; ++i)
    {
        if(p_Event_Date->event_id == event_desc[i].evt_id)
        {
            p_Event_Date->event_type = event_desc[i].evt_class;
            LOG_I("[%03d] %d-%02d-%02d %02d:%02d:%02d.%03d : <%s> %s", idx,
                    t.tm_year + 1900, t.tm_mon+1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec,
                    p_Event_Date->ms, event_desc[i].evt_desc, p_Event_Date->val?"动作":"复归");

            if(1 == simple_en){
                show_evt_act_val(p_Event_Date->val_type, p_Event_Date->act_val_buf);
            }
            return RT_EOK;
        }
    }
    LOG_I("Find Event Failed event_class = %d, event_type = %d", p_Event_Date->event_id, p_Event_Date->event_type);
    return -RT_ERROR;
}

/* 查找同一event_id最近的动作事件开始时间*/
static uint32_t find_event_start_time(uint8_t evt_class, uint16_t event_id)
{
    struct Events_Str *p_Events = &Event_Manger[evt_class].Events;
    uint16_t cnt = p_Events->EvtDetail.eventCnt;

    int idx = p_Events->EvtDetail.tailIdx;

    for (uint16_t i = 0; i < cnt; i++) {
        struct Event_Str *pEvt = &p_Events->Event[idx];

        if (pEvt->event_id == event_id && pEvt->val == 1) {
            return (uint32_t)pEvt->times * 1000 + pEvt->ms;
        }

        idx = (idx + MAX_EVENT - 1) % MAX_EVENT; // 向前遍历
    }
    return 0;
}

/* 保存事件到结构体中 */
void Save_Event(struct Event_Str *p_Event_Date)
{
    uint8_t  event_class = p_Event_Date->event_type;
    struct Events_Str *p_Events = &Event_Manger[event_class].Events;

    //计算持续时间
    if(p_Event_Date->val == 0)
    {
        uint32_t start_time = find_event_start_time(event_class, p_Event_Date->event_id);

        if(start_time != 0)
        {
            uint32_t end_time = (uint32_t)p_Event_Date->times * 1000 + p_Event_Date->ms;

            if(end_time > start_time)
                p_Event_Date->duration_ms = end_time - start_time;
            else
                p_Event_Date->duration_ms = 0;
        }
    }
    else
    {
        /* 动作事件 duration 置 0 */
        p_Event_Date->duration_ms = 0;
    }

    rt_memcpy(&p_Events->Event[p_Events->EvtDetail.tailIdx], p_Event_Date, sizeof(struct Event_Str));


    if(MAX_EVENT == p_Events->EvtDetail.eventCnt)
    {
        p_Events->EvtDetail.tailIdx = (p_Events->EvtDetail.tailIdx + 1) % MAX_EVENT;
        p_Events->EvtDetail.headIdx = (p_Events->EvtDetail.headIdx + 1) % MAX_EVENT;
    }
    else {
        p_Events->EvtDetail.tailIdx = (p_Events->EvtDetail.tailIdx + 1) % MAX_EVENT;
        p_Events->EvtDetail.eventCnt++;
    }
}

/* 事件处理线程 */
void Event_Deal_Loop(void *parameter)
{
    struct Event_Str Event_Date;
    uint8_t Event_Save_Class_Bits_Flg = 0;
    rt_thread_mdelay(4000);
    while(1)
    {
        Event_Save_Class_Bits_Flg = 0;
        if (rt_mq_recv(&Event_mq, &Event_Date, sizeof(Event_Date), RT_WAITING_FOREVER) == RT_EOK)
        {
            Show_Event(&Event_Date, 0, 1);
            if(is_power_down() && !is_power_down_legal_work()){
                LOG_W("Power Down! Do not save new event");
                continue;
            }

            Save_Event(&Event_Date);
            // 记录需要保存的事件类型
            Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Event_Date.event_type);

            // 尝试取出队列里剩余消息（非阻塞）
            while (rt_mq_recv(&Event_mq, &Event_Date, sizeof(Event_Date), 0) == RT_EOK)
            {
                Show_Event(&Event_Date, 0, 1);
                Save_Event(&Event_Date);
                // 记录需要保存的事件类型
                Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Event_Date.event_type);
            }

            // 队列空了再统一写 Flash
            for (uint16_t i = 0; i < EVT_CLASS_MAX; ++i) {
                if(Event_Save_Class_Bits_Flg & (1 << i)){
                    Save_Events_To_Flash(i);
                }
            }
        }
    }
}

int get_next_event_idx(int evt_class, int cur_idx)
{
    struct Events_Str *p_Events = &Event_Manger[evt_class].Events;
    if(cur_idx == -1)
        cur_idx = p_Events->EvtDetail.tailIdx;
    int i = (cur_idx)%MAX_EVENT;

    if(p_Events->EvtDetail.tailIdx == (i + 1)%MAX_EVENT){
        i = p_Events->EvtDetail.headIdx;
        return i;
    }

    return (i + 1)%MAX_EVENT;
}

/* 根据当前事件下标获取旧一个事件 */
int get_prev_event_idx(int evt_class, int cur_idx)
{
    struct Events_Str *p_Events = &Event_Manger[evt_class].Events;

    if(cur_idx == -1)
        cur_idx = p_Events->EvtDetail.tailIdx;
    int i = (cur_idx)%MAX_EVENT;

    if(p_Events->EvtDetail.headIdx == cur_idx){
        i = p_Events->EvtDetail.tailIdx;
    }


    return (MAX_EVENT-1+i)%MAX_EVENT;
}

/* 获取事件个数 */
uint16_t get_event_num(int evt_class)
{
    struct Events_Str *p_Events = &Event_Manger[evt_class].Events;
    return p_Events->EvtDetail.eventCnt;
}

struct Event_Str* get_event(int evt_class, int event_page_idx)
{
    if(event_page_idx>= MAX_EVENT)
        event_page_idx = 0;
    struct Events_Str *p_Events = &Event_Manger[evt_class].Events;
    return &p_Events->Event[event_page_idx];


}

struct Event_Str *Get_Prev_Event(int evt_class, int cur_idx)
{
    uint16_t LastIdx = ((cur_idx + MAX_EVENT) - 1) % MAX_EVENT;
    return &Event_Manger[evt_class].Events.Event[LastIdx];
}

/* 打印一类事件 */
int32_t Show_Class_Events(uint8_t  event_class)
{
    struct Event_Str *p_Event = RT_NULL;
    struct Events_Str *p_Events = &Event_Manger[event_class].Events;
    uint16_t LastIdx = p_Events->EvtDetail.tailIdx; // 最新事件记录下标
    uint16_t eventCnt = p_Events->EvtDetail.eventCnt;

    LOG_I("%s[%03d]:", Event_Manger[event_class].Event_Name, eventCnt);
    for(uint16_t i=0; i<eventCnt; ++i){
        p_Event = Get_Prev_Event(event_class, LastIdx);

        Show_Event(p_Event, i+1, 0);
        LastIdx = ((LastIdx + MAX_EVENT) - 1) % MAX_EVENT;
    }
    return RT_EOK;
}

void Show_All_Events(int argc, char **argv)
{
    if(1 == argc)
    {
        for (uint16_t i = 0; i < EVT_CLASS_MAX; ++i){
            Show_Class_Events(i);
        }
    }
    else if(2 == argc){
        uint16_t event_class = atoi(argv[1]);
        Show_Class_Events(event_class);
    }
}
MSH_CMD_EXPORT(Show_All_Events, Show_All_Events);





