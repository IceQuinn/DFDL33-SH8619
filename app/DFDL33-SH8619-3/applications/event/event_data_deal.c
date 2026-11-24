#include "event_data_deal.h"
#include "user_ex_flash_mgmt.h"
#include <rtthread.h>
#include <string.h>
#include "drv_ex_flash.h"
#include <time.h>

#include "inv_data.h"
#include "dlt645_data_api.h"
#include <stdlib.h>
#include "sys.h"

#define EVENT_DATA_VER              1

#define DBG_TAG "event_data_deal"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


struct Event_Management_Str Event_Management[EVT_CLASS_MAX_] =
{
    {.Event_Flash_Addr_A = FLASH_PV_INV_ONOFF_CTRL_EVENT_A_ADDR,        .Event_Flash_Addr_B = FLASH_PV_INV_ONOFF_CTRL_EVENT_B_ADDR,         .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_ONOFF_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_P_CTRL_EVENT_A_ADDR,            .Event_Flash_Addr_B = FLASH_PV_INV_P_CTRL_EVENT_B_ADDR,             .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_P_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_Q_CTRL_EVENT_A_ADDR,            .Event_Flash_Addr_B = FLASH_PV_INV_Q_CTRL_EVENT_B_ADDR,             .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_Q_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_PF_CTRL_EVENT_A_ADDR,           .Event_Flash_Addr_B = FLASH_PV_INV_PF_CTRL_EVENT_B_ADDR,            .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_PF_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_P_PERIOD_CTRL_EVENT_A_ADDR,     .Event_Flash_Addr_B = FLASH_PV_INV_P_PERIOD_CTRL_EVENT_B_ADDR,      .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_P_PERIOD_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_VOLT_OVER_LIMIT_EVENT_A_ADDR,          .Event_Flash_Addr_B = FLASH_VOLT_OVER_LIMIT_EVENT_B_ADDR,           .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "VOLT_OVER_LIMIT_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_COMM_ERR_EVENT_A_ADDR,          .Event_Flash_Addr_B = FLASH_PV_INV_COMM_ERR_EVENT_B_ADDR,           .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_COMM_ERR_EVENT"},
};

uint8_t Events_Cache_Date[EVENTS_CACHE_NUM_MAX][ONE_EVENT_RECORD_SIZE];// 事件
/* 消息队列控制块 */
static struct rt_messagequeue Event_Data_mq;

/* 全局互斥锁 */
static struct rt_mutex event_mgmt_mutex;

/* 初始化 */
void event_mgmt_mutex_init(void)
{
    rt_mutex_init(&event_mgmt_mutex, "evt_mgmt", RT_IPC_FLAG_FIFO);
}


int32_t Send_Event_Data_To_Cache_By_Type(uint16_t event_class, void *p_act_val)
{
    uint8_t Events_Cache_Data[ONE_EVENT_RECORD_SIZE] = {0};
    Events_Cache_Data[0] = event_class;
    memcpy(&Events_Cache_Data[1], p_act_val, ONE_EVENT_RECORD_SIZE - 1);

    if(RT_EOK != rt_mq_send(&Event_Data_mq, &Events_Cache_Data, sizeof(Events_Cache_Data)))
    {
        rt_kprintf("Event Send Error\n");
        return -RT_ERROR;
    }
    return 0;
}

void Get_EventTime(EventTime *evt_time)
{
    time_t now = time(RT_NULL);
    struct tm *p_tm = localtime(&now);

    evt_time->year   = p_tm->tm_year % 100;
    evt_time->month  = p_tm->tm_mon + 1;
    evt_time->day    = p_tm->tm_mday;
    evt_time->hour   = p_tm->tm_hour;
    evt_time->minute = p_tm->tm_min;
    evt_time->second = p_tm->tm_sec;
}


int32_t Event_Data_Cache_Init(void)
{
    int32_t result;

    /* 初始化消息队列 */
    result = rt_mq_init(&Event_Data_mq,
                        "Event_Data_mq",
                        Events_Cache_Date,               /* 内存池指向 msg_pool */
                        sizeof(Events_Cache_Date[0]),    /* 每个消息的大小是 1 字节 */
                        sizeof(Events_Cache_Date),       /* 内存池的大小是 msg_pool 的大小 */
                        RT_IPC_FLAG_PRIO);               /* 如果有多个线程等待，优先级大小的方法分配消息 */

    if (result != RT_EOK)
    {
        rt_kprintf("init Event message queue failed.\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

void Show_Event_Data(uint8_t event_class, void *p_evt_data)
{
    if(p_evt_data == NULL){
        LOG_W("Show_Event: p_evt_data is NULL");
        return;
    }

    if(event_class >= EVT_CLASS_MAX_){
        LOG_W("Show_Event: invalid event_class=%d", event_class);
        return;
    }

    LOG_I("=== %s (class=%d) ===",
          Event_Management[event_class].Event_Name, event_class);

    switch(event_class)
    {
        /* ---------- 电压越限 ---------- */
        case ETP_VOLT_OVER_LIMIT:{
            VoltageOverLimit_t *p = (VoltageOverLimit_t *)p_evt_data;
            LOG_I("Start: 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
            LOG_I("VolA : over=%-5d recover=%-5d (0.1V)",
                  p->over_limit_vol_a, p->recover_vol_a);
            LOG_I("VolB : over=%-5d recover=%-5d (0.1V)",
                  p->over_limit_vol_b, p->recover_vol_b);
            LOG_I("VolC : over=%-5d recover=%-5d (0.1V)",
                  p->over_limit_vol_c, p->recover_vol_c);
            break;
        }

        /* ---------- 功率控制类 ---------- */
        case ETP_PV_INV_P_CTRL:
        case ETP_PV_INV_Q_CTRL:
        case ETP_PV_INV_P_PERIOD_CTRL:{
            PowerControlEvent_t *p = (PowerControlEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time    : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
            LOG_I("Ctrl Type : type=%-3d", p->event_type);

            /* 逆变器编号行 */
            rt_kprintf("Inv   :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                rt_kprintf(" %d\t", i+1);
            }
            rt_kprintf("\n");

            /* 调控结果行（文字映射） */
            rt_kprintf("Result:");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->control_result[i] == 0){
                    rt_kprintf("成功\t");
                }else if (p->control_result[i] == 1){
                    rt_kprintf("未调控\t ");
                }else if (p->control_result[i] == 2){
                    rt_kprintf("失败\t");
                }else if (p->control_result[i] == 3){
                    rt_kprintf("不支持\t");
                }else if (p->control_result[i] == 4){
                    rt_kprintf("其它\t");
                }else if (p->control_result[i] == 5){
                    rt_kprintf("无档案\t");
                }else{
                    rt_kprintf("%d\t", p->control_result[i]);
                }
            }
            rt_kprintf("\n");

            /* 调控值行（无效值显示 --） */
            rt_kprintf("Value :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->control_value[i] == INT32_MIN){
                    rt_kprintf(" --\t");
                }else{
                    rt_kprintf("%d\t", p->control_value[i]);
                }
            }
            rt_kprintf("\n");
            break;
        }

        /* ---------- 功率因数 ---------- */
        case ETP_PV_INV_PF_CTRL:{
            PowerFactorEvent_t *p = (PowerFactorEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time    : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);

            /* 逆变器编号行 */
            rt_kprintf("Inv   :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                rt_kprintf(" %d\t", i+1);
            }
            rt_kprintf("\n");

            /* 调控结果行 */
            rt_kprintf("Result:");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->control_result[i] == 0){
                    rt_kprintf("成功\t");
                }else if(p->control_result[i] == 1){
                    rt_kprintf("未调控\t");
                }else if(p->control_result[i] == 2){
                    rt_kprintf("失败\t");
                }else if(p->control_result[i] == 3){
                    rt_kprintf("不支持\t");
                }else if(p->control_result[i] == 4){
                    rt_kprintf("其它\t");
                }else if(p->control_result[i] == 5){
                    rt_kprintf("无档案\t");
                }else{
                    rt_kprintf("%d\t", p->control_result[i]);
                }
            }
            rt_kprintf("\n");

            /* 调控值行（无效值显示 --）；inv_values为int16，无值哨兵为INT16_MIN。 */
            rt_kprintf("Value :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->inv_values[i] == INT16_MIN){
                    rt_kprintf(" --\t");
                }else{
                    rt_kprintf("%d\t", p->inv_values[i]);
                }
            }
            rt_kprintf("\n");
            break;
        }

        /* ---------- 开关机控制 ---------- */
        case ETP_PV_INV_ONOFF_CTRL:{
            SwitchEvent_t *p = (SwitchEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time    : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);

            /* 逆变器编号行 */
            rt_kprintf("Inv   :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                rt_kprintf("%d\t", i+1);
            }
            rt_kprintf("\n");

            /* 开关机类型行 */
            rt_kprintf("Type  :");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->event_type[i] == 0){
                    rt_kprintf("ON\t");
                }else if (p->event_type[i] == 1){
                    rt_kprintf("OFF\t");
                }else{
                    rt_kprintf("--\t");
                }
            }
            rt_kprintf("\n");

            /* 调控结果行 */
            rt_kprintf("Result:");
            for(int i = 0; i < EVENT_MAX_INVERTER_NUM; i++){
                if(p->control_result[i] == 0){
                    rt_kprintf("成功\t");
                }else if(p->control_result[i] == 1){
                    rt_kprintf("未调控\t");
                }else if(p->control_result[i] == 2){
                    rt_kprintf("失败\t");
                }else if(p->control_result[i] == 3){
                    rt_kprintf("不支持\t");
                }else if(p->control_result[i] == 4){
                    rt_kprintf("其它\t");
                }else if(p->control_result[i] == 5){
                    rt_kprintf("无档案\t");
                } else {
                    rt_kprintf("%d\t", p->control_result[i]);
                }
            }
            rt_kprintf("\n");
            break;
        }

        /* ---------- 通信异常 ---------- */
        case ETP_PV_INV_COMM_ERR:{
            CommErrEvent_t *p = (CommErrEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time    : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
            LOG_I("PV Inverter Id  : %d", p->inv_id + 1);
            break;
        }

        /* ---------- 默认：十六进制 ---------- */
        default:{
            LOG_I("Data  :");
            for(int i = 0; i < 24; i++){
                LOG_I(" %02X", ((uint8_t *)p_evt_data)[i]);
            }
            break;
        }
    }
}

/**
 * @brief 打印各类事件保存的总记录条数
 */
void Show_Event_Total_Count(void)
{
    rt_mutex_take(&event_mgmt_mutex, RT_WAITING_FOREVER);

    LOG_I("========== Event Total Count ==========");

    for(uint8_t cls = 0; cls < EVT_CLASS_MAX_; cls++)
    {
        struct Event_Digest_Str *p_digest = (struct Event_Digest_Str *)Event_Management[cls].buf;

        LOG_I("%-30s : %d / %d",
              Event_Management[cls].Event_Name,
              p_digest->eventCnt,
              Event_Management[cls].Event_Max_Cnt);
    }

    LOG_I("======================================");

    rt_mutex_release(&event_mgmt_mutex);
}
MSH_CMD_EXPORT(Show_Event_Total_Count, Show event total count);

/**
 * @brief 按事件类型由新到旧打印该类型的所有记录
 * @param event_class 事件类型枚举值
 */
void Show_Event_Records_By_Type(uint8_t event_class)
{
    struct Event_Digest_Str *p_digest;
    uint8_t *p_base;
    uint16_t max_cnt;
    uint16_t actual_idx;
    uint16_t seq;

    if (event_class >= EVT_CLASS_MAX_)
    {
        LOG_W("Invalid event_class=%d, max=%d", event_class, EVT_CLASS_MAX_);
        return;
    }

    rt_mutex_take(&event_mgmt_mutex, RT_WAITING_FOREVER);

    p_digest = (struct Event_Digest_Str *)Event_Management[event_class].buf;
    max_cnt  = Event_Management[event_class].Event_Max_Cnt;

    /* ---------- 标题 ---------- */
    rt_kprintf("========== %s ==========\n",
               Event_Management[event_class].Event_Name);
    rt_kprintf("事件总次数：%d\n", p_digest->eventCnt);

    if(p_digest->eventCnt == 0)
    {
        rt_kprintf("(无记录)\n");
        rt_kprintf("===================================================\n");
        rt_mutex_release(&event_mgmt_mutex);
        return;
    }

    /* ---------- 表头 ---------- */
    rt_kprintf("序号\t开始时间\t\t结束时间\n");

    /* ---------- 记录基址 ---------- */
    p_base = &Event_Management[event_class].buf[
                sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str) + 24];

    /* ---------- 由新到旧遍历 ---------- */
    for(seq = 1; seq <= p_digest->eventCnt; seq++)
    {
        actual_idx = (p_digest->tailIdx + max_cnt - seq) % max_cnt;

        switch(event_class)
        {
            case ETP_PV_INV_ONOFF_CTRL:{
                SwitchEvent_t *p = (SwitchEvent_t *)(p_base + actual_idx * sizeof(SwitchEvent_t));
                rt_kprintf("%d\t20%02d-%02d-%02d %02d:%02d:%02d\t20%02d-%02d-%02d %02d:%02d:%02d\n",
                           seq,
                           p->start_time.year, p->start_time.month, p->start_time.day,
                           p->start_time.hour, p->start_time.minute, p->start_time.second,
                           p->end_time.year, p->end_time.month, p->end_time.day,
                           p->end_time.hour, p->end_time.minute, p->end_time.second);
                break;
            }
            case ETP_PV_INV_P_CTRL:
            case ETP_PV_INV_Q_CTRL:
            case ETP_PV_INV_P_PERIOD_CTRL:{
                PowerControlEvent_t *p = (PowerControlEvent_t *)(p_base + actual_idx * sizeof(PowerControlEvent_t));
                rt_kprintf("%d\t20%02d-%02d-%02d %02d:%02d:%02d\t20%02d-%02d-%02d %02d:%02d:%02d\n",
                           seq,
                           p->start_time.year, p->start_time.month, p->start_time.day,
                           p->start_time.hour, p->start_time.minute, p->start_time.second,
                           p->end_time.year, p->end_time.month, p->end_time.day,
                           p->end_time.hour, p->end_time.minute, p->end_time.second);
                break;
            }
            case ETP_PV_INV_PF_CTRL:{
                PowerFactorEvent_t *p = (PowerFactorEvent_t *)(p_base + actual_idx * sizeof(PowerFactorEvent_t));
                rt_kprintf("%d\t20%02d-%02d-%02d %02d:%02d:%02d\t20%02d-%02d-%02d %02d:%02d:%02d\n",
                           seq,
                           p->start_time.year, p->start_time.month, p->start_time.day,
                           p->start_time.hour, p->start_time.minute, p->start_time.second,
                           p->end_time.year, p->end_time.month, p->end_time.day,
                           p->end_time.hour, p->end_time.minute, p->end_time.second);
                break;
            }
            case ETP_VOLT_OVER_LIMIT:{
                VoltageOverLimit_t *p = (VoltageOverLimit_t *)(p_base + actual_idx * sizeof(VoltageOverLimit_t));
                rt_kprintf("%d\t20%02d-%02d-%02d %02d:%02d:%02d\t20%02d-%02d-%02d %02d:%02d:%02d\n",
                           seq,
                           p->start_time.year, p->start_time.month, p->start_time.day,
                           p->start_time.hour, p->start_time.minute, p->start_time.second,
                           p->end_time.year, p->end_time.month, p->end_time.day,
                           p->end_time.hour, p->end_time.minute, p->end_time.second);
                break;
            }
            case ETP_PV_INV_COMM_ERR:{
                CommErrEvent_t *p = (CommErrEvent_t *)(p_base + actual_idx * sizeof(CommErrEvent_t));
                rt_kprintf("%d\t20%02d-%02d-%02d %02d:%02d:%02d\t20%02d-%02d-%02d %02d:%02d:%02d\n",
                           seq,
                           p->start_time.year, p->start_time.month, p->start_time.day,
                           p->start_time.hour, p->start_time.minute, p->start_time.second,
                           p->end_time.year, p->end_time.month, p->end_time.day,
                           p->end_time.hour, p->end_time.minute, p->end_time.second);
                break;
            }
            default:
                rt_kprintf("%d\t(unknown)\n", seq);
                break;
        }
    }

    rt_kprintf("===================================================\n");

    rt_mutex_release(&event_mgmt_mutex);
}

static int show_event_data_by_type(int argc, char **argv)
{
    uint8_t cls;

    if(argc != 2)
    {
        rt_kprintf("Usage: show_event_data_by_type <event_class>\n");
        rt_kprintf("event_class=0 逆变器开关机控制事件\n");
        rt_kprintf("event_class=1 逆变器有功控制事件\n");
        rt_kprintf("event_class=2 逆变器无功控制事件\n");
        rt_kprintf("event_class=3 逆变器功率因数控制事件\n");
        rt_kprintf("event_class=4 逆变器有功时段控制事件\n");
        rt_kprintf("event_class=5 装置电压越限事件\n");
        rt_kprintf("event_class=6 逆变器通信异常事件\n");
        return -1;
    }

    cls = (uint8_t)atoi(argv[1]);
    Show_Event_Records_By_Type(cls);
    return 0;
}
MSH_CMD_EXPORT(show_event_data_by_type, Show event data by type [0~6]);

void Save_Event_Data(uint8_t *p_Event_Date)
{
    
    uint8_t event_class = p_Event_Date[0];
    struct Event_Digest_Str *p_evt_digest = Event_Management[event_class].buf;
    struct Event_Desc_Str *p_evt_desc = Event_Management[event_class].buf[sizeof(struct Event_Digest_Str)];
    p_evt_desc->evt_class = event_class;
    uint8_t *p_buf = &Event_Management[event_class].buf[sizeof(struct Event_Digest_Str)+sizeof(struct Event_Desc_Str)];


    struct Event_Management_Str *p_Event_Management = &Event_Management[event_class];

    uint16_t tailIdx = p_evt_digest->tailIdx;   // flash记录中，最后一条记录的启动偏移
    uint8_t *p_save_addr = NULL;
    uint32_t size = 0;

    if(event_class == ETP_PV_INV_ONOFF_CTRL){
        size = sizeof(SwitchEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PV_INV_P_CTRL){
        size = sizeof(PowerControlEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PV_INV_Q_CTRL){
        size = sizeof(PowerControlEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PV_INV_PF_CTRL){
        size = sizeof(PowerFactorEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PV_INV_P_PERIOD_CTRL){
        size = sizeof(PowerControlEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_VOLT_OVER_LIMIT){
        size = sizeof(VoltageOverLimit_t);
        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PV_INV_COMM_ERR){
        size = sizeof(CommErrEvent_t);
        p_save_addr = &p_buf[24 + tailIdx * size]; // 事件数据缓存区
    }

    rt_memcpy(p_save_addr, &p_Event_Date[1], size);

    if(p_Event_Management->Event_Max_Cnt == p_evt_digest->eventCnt)
    {
        p_evt_digest->tailIdx = (p_evt_digest->tailIdx + 1) % p_Event_Management->Event_Max_Cnt;
        p_evt_digest->headIdx = (p_evt_digest->headIdx + 1) % p_Event_Management->Event_Max_Cnt;
    }
    else
    {
        p_evt_digest->tailIdx = (p_evt_digest->tailIdx + 1) % p_Event_Management->Event_Max_Cnt;
        p_evt_digest->eventCnt++;
    }
}

/* 根据事件类型保存事件到Flash */
void Save_Event_Data_To_Flash(uint8_t event_class)
{
    AB_save(flash_write,
            Event_Management[event_class].Event_Flash_Addr_A,
            Event_Management[event_class].Event_Flash_Addr_B,
            &Event_Management[event_class].buf,
            EVENT_DATA_VER,
            sizeof(Event_Management[event_class].buf),
            Event_Management[event_class].Event_Name);
}

uint8_t Clear_Event_Data(uint8_t  event_class)
{
    if(event_class >= EVT_CLASS_MAX_)
    {
        return RT_EINVAL;
    }

    rt_mutex_take(&event_mgmt_mutex, RT_WAITING_FOREVER);

    rt_memset(&Event_Management[event_class].buf, 0, sizeof(Event_Management[event_class].buf));
    rt_kprintf("%s has been Clear\n", Event_Management[event_class].Event_Name);

    struct Event_Digest_Str *p_digest =
                (struct Event_Digest_Str *)Event_Management[event_class].buf;
    p_digest->eventCnt = 0;
    p_digest->headIdx  = 0;
    p_digest->tailIdx  = 0;

    Save_Event_Data_To_Flash(event_class);

    rt_mutex_release(&event_mgmt_mutex);

    return RT_EOK;
}

void Clear_All_Event_Data(void)
{
    for(uint8_t i = ETP_PV_INV_ONOFF_CTRL; i < EVT_CLASS_MAX_; i++)
    {
        if(RT_EOK != Clear_Event_Data(i))
        {
            rt_kprintf("Clear Event Data Failed, Event Id: %d", i);
        }
    }
}
MSH_CMD_EXPORT(Clear_All_Event_Data, Clear All Event Data);

/* 事件初始化 */
void Event_Data_Init(void)
{
    int32_t check_sta = 0;
    Event_Data_Cache_Init();

    event_mgmt_mutex_init();

    for (uint16_t i = 0; i < EVT_CLASS_MAX_; ++i)
    {
        check_sta = AB_check(flash_read,
                            flash_write,
                            Event_Management[i].Event_Flash_Addr_A,
                            Event_Management[i].Event_Flash_Addr_B,
                            &Event_Management[i].buf,
                            sizeof(Event_Management[i].buf),
                            Event_Management[i].Event_Name);

        if(1 == check_sta)
        {
            Clear_Event_Data(i);
        }
    }
}

void Event_Data_Deal_Loop(void *parameter)
{
    uint8_t  Events_Cache_Data[ONE_EVENT_RECORD_SIZE] = {0};
    uint16_t Event_Save_Class_Bits_Flg = 0;



    rt_thread_mdelay(4000);
    while(1)
    {
        Event_Save_Class_Bits_Flg = 0;
        if (rt_mq_recv(&Event_Data_mq, Events_Cache_Data, sizeof(Events_Cache_Data), RT_WAITING_FOREVER) == RT_EOK)
        {
            Show_Event_Data(Events_Cache_Data[0], &Events_Cache_Data[1]);
            if(is_power_down() && !is_power_down_legal_work()){
                LOG_W("Power Down! Do not save new event data");
                continue;
            }

            rt_mutex_take(&event_mgmt_mutex, RT_WAITING_FOREVER);

            Save_Event_Data(Events_Cache_Data);

            rt_mutex_release(&event_mgmt_mutex);
            // 记录需要保存的事件类型
            Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Events_Cache_Data[0]);

            // 尝试取出队列里剩余消息（非阻塞）
            while (rt_mq_recv(&Event_Data_mq, Events_Cache_Data, sizeof(Events_Cache_Data), 0) == RT_EOK)
            {
                Show_Event_Data(Events_Cache_Data[0], &Events_Cache_Data[1]);

                rt_mutex_take(&event_mgmt_mutex, RT_WAITING_FOREVER);
                Save_Event_Data(Events_Cache_Data);
                rt_mutex_release(&event_mgmt_mutex);
                // 记录需要保存的事件类型
                Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Events_Cache_Data[0]);
            }

            // 队列空了再统一写 Flash
            for (uint16_t i = 0; i < EVT_CLASS_MAX_; ++i) {
                if(Event_Save_Class_Bits_Flg & (1 << i)){
                    Save_Event_Data_To_Flash(i);
                }
            }
        }
    }
}

//开关机
void Send_Inv_OnOff_Event_Data(EventTime start_time, EventTime end_time,
                               uint8_t *ctrl_type, uint8_t *ctrl_result)
{
    uint16_t event = ETP_PV_INV_ONOFF_CTRL;

    /* ---------- 计数器指针 ---------- */
    uint16_t *p_cnt = (uint16_t *)&Event_Management[event].buf[
                        sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    SwitchEvent_t SwitchEvent;
    rt_memset(&SwitchEvent, 0, sizeof(SwitchEvent_t));

    SwitchEvent.start_time = start_time;
    SwitchEvent.end_time   = end_time;

    for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
    {
        SwitchEvent.event_type[i]      = ctrl_type[i];
        SwitchEvent.control_result[i]  = ctrl_result[i];

        /* 只有下发值为全FF（01未调控）才不计次；00成功、02失败、03不支持、04其他问题都算一次调控意图，
         * 包括没有档案的情况——主站实际下发了调控值，事件里也保留了该值。 */
        if(ctrl_result[i] != DLT645_CONTROL_STATUS_NOT_CONTROLLED)
        {
            if (p_cnt[i] < Event_Management[event].Event_Max_Cnt)
            {
                p_cnt[i]++;
            }
        }
    }
    Send_Event_Data_To_Cache_By_Type(event, &SwitchEvent);
}

//功率
void Send_PowerEvent_Data(EventTime start_time, EventTime end_time,
                          uint8_t ctrl_type, uint8_t *ctrl_result, int32_t *ctrl_value)
{
    uint16_t event = 0;
    uint8_t  event_type = 0;

    if(0xff == ctrl_type)
    {
        rt_kprintf("Control type exception!The event is invalid.\n");
        return ;
    }

    if(ctrl_type == INV_CONTROL_ACTIVE_POWER)
    {
        event = ETP_PV_INV_P_CTRL;
        event_type = 0;
    }
    else if(ctrl_type == INV_CONTROL_ACTIVE_POWER_PERCENT)
    {
        event = ETP_PV_INV_P_CTRL;
        event_type = 1;
    }
    else if(ctrl_type == INV_CONTROL_REACTIVE_POWER)
    {
        event = ETP_PV_INV_Q_CTRL;
        event_type = 0;
    }
    else if(ctrl_type == INV_CONTROL_REACTIVE_POWER_PERCENT)
    {
        event = ETP_PV_INV_Q_CTRL;
        event_type = 1;
    }
    else if(ctrl_type == INV_CONTROL_POWER_FACTOR)
    {
        event = ETP_PV_INV_PF_CTRL;
    }

    /* ---------- 计数器指针 ---------- */
    uint16_t *p_cnt = (uint16_t *)&Event_Management[event].buf[
                        sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    /* ---------- 功率因数控制 ---------- */
    if(ctrl_type == INV_CONTROL_POWER_FACTOR)
    {
        PowerFactorEvent_t PowerFactorEvent;
        rt_memset(&PowerFactorEvent, 0, sizeof(PowerFactorEvent_t));

        PowerFactorEvent.start_time = start_time;
        PowerFactorEvent.end_time   = end_time;

        for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
        {
            PowerFactorEvent.control_result[i] = ctrl_result[i];
            /* 未参与或值非法的槽位用INT16_MIN表示无值；功率因数645值域为-1000～1000，不会与该哨兵冲突。 */
            if(ctrl_value[i] == INT32_MIN)
            {
                PowerFactorEvent.inv_values[i] = INT16_MIN;
            }
            else if((ctrl_value[i] < (int32_t)INT16_MIN) || (ctrl_value[i] > (int32_t)INT16_MAX))
            {
                PowerFactorEvent.inv_values[i] = INT16_MIN; /* 超出int16表示范围时按无值处理，避免截断产生错误数值。 */
            }
            else
            {
                PowerFactorEvent.inv_values[i] = (int16_t)ctrl_value[i];
            }

            /* 只有下发值为全FF（01未调控）才不计次；00成功、02失败、03不支持、04其他问题都算一次调控意图，
             * 包括没有档案和厂家精度无法承载的情况，两者都代表本次存在调控意图。 */
            if(ctrl_result[i] != DLT645_CONTROL_STATUS_NOT_CONTROLLED)
            {
                if(p_cnt[i] < Event_Management[event].Event_Max_Cnt)
                {
                    p_cnt[i]++;
                }
            }
        }
        Send_Event_Data_To_Cache_By_Type(event, &PowerFactorEvent);
    }
    /* ---------- 有功/无功 ---------- */
    else
    {
        PowerControlEvent_t PowerControlEvent;
        rt_memset(&PowerControlEvent, 0, sizeof(PowerControlEvent_t));

        PowerControlEvent.start_time = start_time;
        PowerControlEvent.end_time   = end_time;
        PowerControlEvent.event_type = event_type;

        for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
        {
            PowerControlEvent.control_result[i] = ctrl_result[i];
            PowerControlEvent.control_value[i]  = ctrl_value[i];

            /* 只有下发值为全FF（01未调控）才不计次；00成功、02失败、03不支持、04其他问题都算一次调控意图，
             * 包括没有档案和厂家精度无法承载的情况，两者都代表本次存在调控意图。 */
            if(ctrl_result[i] != DLT645_CONTROL_STATUS_NOT_CONTROLLED)
            {
                if(p_cnt[i] < Event_Management[event].Event_Max_Cnt)
                {
                    p_cnt[i]++;
                }
            }
        }
        Send_Event_Data_To_Cache_By_Type(event, &PowerControlEvent);
    }
}

void Send_PowerTimeControlEvent_Data(EventTime start_time, EventTime end_time,
                          uint8_t ctrl_type, uint8_t *ctrl_result, int32_t *ctrl_value)
{
    uint16_t event = ETP_PV_INV_P_PERIOD_CTRL;

    /* ---------- 计数器指针 ---------- */
    uint16_t *p_cnt = (uint16_t *)&Event_Management[event].buf[
                        sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    PowerControlEvent_t PowerControlEvent;
    rt_memset(&PowerControlEvent, 0, sizeof(PowerControlEvent_t));

    PowerControlEvent.start_time = start_time;
    PowerControlEvent.end_time   = end_time;
    PowerControlEvent.event_type = ctrl_type;

    for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
    {
        PowerControlEvent.control_result[i] = ctrl_result[i];
        PowerControlEvent.control_value[i]  = ctrl_value[i];

        /* 只有下发值为全FF（01未调控）才不计次；00成功、02失败、03不支持、04其他问题都算一次调控意图，
         * 包括没有档案的情况——主站实际下发了调控值，事件里也保留了该值。 */
        if(ctrl_result[i] != DLT645_CONTROL_STATUS_NOT_CONTROLLED)
        {
            if(p_cnt[i] < Event_Management[event].Event_Max_Cnt)
            {
                p_cnt[i]++;
            }
        }
    }
    Send_Event_Data_To_Cache_By_Type(event, &PowerControlEvent);
}


/* 电压越限事件发送 */
void Send_VoltOverLimitEvent_Data(EventTime start_time, EventTime end_time,
                                  uint16_t over_vol_a, uint16_t over_vol_b, uint16_t over_vol_c,
                                  uint16_t recover_vol_a, uint16_t recover_vol_b, uint16_t recover_vol_c)
{
    VoltageOverLimit_t volt_event;
    uint8_t *p;

    rt_memset(&volt_event, 0, sizeof(VoltageOverLimit_t));
    volt_event.start_time = start_time;
    volt_event.end_time   = end_time;
    volt_event.over_limit_vol_a = over_vol_a;
    volt_event.over_limit_vol_b = over_vol_b;
    volt_event.over_limit_vol_c = over_vol_c;
    volt_event.recover_vol_a    = recover_vol_a;
    volt_event.recover_vol_b    = recover_vol_b;
    volt_event.recover_vol_c    = recover_vol_c;

    /* 总次数计数 */
    p = &Event_Management[ETP_VOLT_OVER_LIMIT].buf[
            sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];
    if(*p < Event_Management[ETP_VOLT_OVER_LIMIT].Event_Max_Cnt)
    {
        (*p)++;
    }

    Send_Event_Data_To_Cache_By_Type(ETP_VOLT_OVER_LIMIT, &volt_event);
}

/* 通信异常事件数据发送 */
void Send_CommErrEvent_Data(EventTime start_time, EventTime end_time, uint8_t inv_id)
{
    CommErrEvent_t comm_err_event;
    uint16_t      *p_cnt;

    /* ---------- 参数检查 ---------- */
    if(inv_id >= EVENT_MAX_INVERTER_NUM)
    {
        return;
    }

    /* ---------- 填充事件记录 ---------- */
    rt_memset(&comm_err_event, 0, sizeof(CommErrEvent_t));
    comm_err_event.start_time = start_time;
    comm_err_event.end_time   = end_time;
    comm_err_event.inv_id     = inv_id;

    /* ---------- 递增对应逆变器的通信异常次数 ---------- */
    /* buf布局: Event_Digest_Str + Event_Desc_Str + comm_err_event_cnt[EVENT_MAX_INVERTER_NUM] */
    p_cnt = (uint16_t *)&Event_Management[ETP_PV_INV_COMM_ERR].buf[
                sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    if(p_cnt[inv_id] < Event_Management[ETP_PV_INV_COMM_ERR].Event_Max_Cnt)
    {
        p_cnt[inv_id]++;
    }

    /* ---------- 发送事件 ---------- */
    Send_Event_Data_To_Cache_By_Type(ETP_PV_INV_COMM_ERR, &comm_err_event);
}



