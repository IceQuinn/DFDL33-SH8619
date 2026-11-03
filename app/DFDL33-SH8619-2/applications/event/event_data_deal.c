#include "event_data_deal.h"
#include "user_ex_flash_mgmt.h"
#include <rtthread.h>
#include <string.h>
#include "drv_ex_flash.h"
#include <time.h>

#include "inv_data.h"

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
//    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_EVENT_CLEAR_EVENT_A_ADDR,   .Event_Flash_Addr_B = FLASH_PROTO_UNIT_EVENT_CLEAR_EVENT_B_ADDR,    .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Event Reset Event"},
//    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_OTA_UPGRADE_EVENT_A_ADDR,   .Event_Flash_Addr_B = FLASH_PROTO_UNIT_OTA_UPGRADE_EVENT_B_ADDR,    .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Upgrading Event"},
//    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_TIME_CALIB_EVENT_A_ADDR,    .Event_Flash_Addr_B = FLASH_PROTO_UNIT_TIME_CALIB_EVENT_A_ADDR,     .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Time Calibration Event"},
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
    if (p_evt_data == NULL) {
        LOG_W("Show_Event: p_evt_data is NULL");
        return;
    }

    if (event_class >= EVT_CLASS_MAX_) {
        LOG_W("Show_Event: invalid event_class=%d", event_class);
        return;
    }

    LOG_I("=== %s (class=%d) ===",
          Event_Management[event_class].Event_Name, event_class);

    switch (event_class) {

        /* ---------- 电压越限 ---------- */
        case ETP_VOLT_OVER_LIMIT: {
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
        case ETP_PV_INV_P_PERIOD_CTRL: {
            PowerControlEvent_t *p = (PowerControlEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
            LOG_I("Ctrl Type : type=%-3d", p->event_type);
            break;
        }

        /* ---------- 功率因数 ---------- */
        case ETP_PV_INV_PF_CTRL: {
            PowerFactorEvent_t *p = (PowerFactorEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
//            LOG_I("PF    : %-6d (0.001)", p->pf_value);
            break;
        }

        /* ---------- 通信异常 ---------- */
        case ETP_PV_INV_COMM_ERR: {
            CommErrEvent_t *p = (CommErrEvent_t *)p_evt_data;
            LOG_I("Start Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->start_time.year, p->start_time.month, p->start_time.day,
                  p->start_time.hour, p->start_time.minute, p->start_time.second);
            LOG_I("End Time  : 20%02d-%02d-%02d %02d:%02d:%02d",
                  p->end_time.year, p->end_time.month, p->end_time.day,
                  p->end_time.hour, p->end_time.minute, p->end_time.second);
            LOG_I("PV Inverter Id  : %d", p->inv_id);
            break;
        }

        /* ---------- 默认：十六进制 dump ---------- */
        default: {
            LOG_I("Data  :");
            for (int i = 0; i < 24; i++) {
                LOG_I(" %02X", ((uint8_t *)p_evt_data)[i]);
            }
            break;
        }
    }
}

// void save_data_by_class(uint8_t event_class, uint8_t *p_data, uint16_t data_len)
// {
//     if(MAX_EVENT == p_Events->EvtDetail.eventCnt)
//     {
//         p_Events->EvtDetail.tailIdx = (p_Events->EvtDetail.tailIdx + 1) % MAX_EVENT;
//         p_Events->EvtDetail.headIdx = (p_Events->EvtDetail.headIdx + 1) % MAX_EVENT;
//     }
//     else {
//         p_Events->EvtDetail.tailIdx = (p_Events->EvtDetail.tailIdx + 1) % MAX_EVENT;
//         p_Events->EvtDetail.eventCnt++;
//     }

//     rt_memcpy(p_Events->buf, p_data, data_len);
// }

void Save_Event_Data(uint8_t *p_Event_Date)
{
    
    uint8_t event_class = p_Event_Date[0];
    struct Event_Digest_Str *p_evt_digest = Event_Management[event_class].buf;
    struct Event_Desc_Str *p_evt_desc = Event_Management[event_class].buf[sizeof(struct Event_Digest_Str)];
    p_evt_desc->evt_class = event_class;
    uint8_t *p_buf = &Event_Management[event_class].buf[sizeof(struct Event_Digest_Str)+sizeof(struct Event_Desc_Str)];


    struct Event_Management_Str *p_Event_Management = &Event_Management[event_class];

//    uint16_t eventCnt = p_evt_digest->eventCnt;  // flash中一共有多少条记录
//    uint16_t headIdx = p_evt_digest->headIdx;   // flash记录中，第一条记录的启动偏移
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
//    else if(event_class == ETP_PROTO_UNIT_EVENT_CLEAR){
//        size = sizeof(EventTime);
//        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
//    }
//    else if(event_class == ETP_PROTO_UNIT_OTA_UPGRADE){
//        size = sizeof(uint16_t);
//        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
//    }
//    else if(event_class == ETP_PROTO_UNIT_TIME_CALIB){
//        size = sizeof(TimeCalibrationEvent_t);
//        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
//    }


    rt_memcpy(p_save_addr, &p_Event_Date[1], size);


    if(p_Event_Management->Event_Max_Cnt == p_evt_digest->eventCnt)
    {
        p_evt_digest->tailIdx = (p_evt_digest->tailIdx + 1) % p_Event_Management->Event_Max_Cnt;
        p_evt_digest->headIdx = (p_evt_digest->headIdx + 1) % p_Event_Management->Event_Max_Cnt;
    }
    else {
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

int32_t Clear_Event_Data(uint8_t  event_class)
{
    if (event_class >= EVT_CLASS_MAX_) {
        return -RT_EINVAL;
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

/* 事件初始化 */
void Event_Data_Init(void)
{
    int32_t check_sta = 0;
    Event_Data_Cache_Init();

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
//    Send_Event_To_Cache(ETP_POWER_UP, EVT_SET, RT_NULL);
}

void Event_Data_Deal_Loop(void *parameter)
{
    uint8_t  Events_Cache_Data[ONE_EVENT_RECORD_SIZE] = {0};
    uint16_t Event_Save_Class_Bits_Flg = 0;

    event_mgmt_mutex_init();

    rt_thread_mdelay(4000);
    while(1)
    {
        Event_Save_Class_Bits_Flg = 0;
        if (rt_mq_recv(&Event_Data_mq, Events_Cache_Data, sizeof(Events_Cache_Data), RT_WAITING_FOREVER) == RT_EOK)
        {
            Show_Event_Data(Events_Cache_Data[0], &Events_Cache_Data[1]);
//             if(is_power_down() && !is_power_down_legal_work()){
//                 LOG_W("Power Down! Do not save new event");
//                 continue;
//             }

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
void Send_Inv_OnOff_Event_Data(EventTime start_time, EventTime end_time, uint8_t  ctrl_type, uint8_t *ctrl_result)
{
    uint16_t event = ETP_PV_INV_ONOFF_CTRL;
    uint8_t  event_type = 0;
//    uint8_t  *ctrl_cnt = 0;

    if(ctrl_type == INV_CONTROL_POWER_ON)
    {
        event_type = 0;
    }
    else if(ctrl_type == INV_CONTROL_POWER_OFF)
    {
        event_type = 1;
    }

    uint8_t *p = &Event_Management[event].buf[sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    SwitchEvent_t SwitchEvent;

    SwitchEvent.start_time = start_time;
    SwitchEvent.end_time = end_time;

    for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
    {
        SwitchEvent.event_type[i] = event_type;
        SwitchEvent.control_result[i] = ctrl_result[i];
        if(1 != SwitchEvent.control_result[i])
        {
            if(p[i] < Event_Management[event].Event_Max_Cnt)
            {
                p[i]++;
            }
        }
    }
    Send_Event_Data_To_Cache_By_Type(event, &SwitchEvent);
}

//功率
void Send_PowerEvent_Data(EventTime start_time, EventTime end_time, uint8_t  ctrl_type, uint8_t *ctrl_result, int32_t *ctrl_value)
{
    uint16_t event = 0;
    uint8_t  event_type = 0;
//    uint8_t  *ctrl_cnt = 0;

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
    else
    {
        event = ETP_PV_INV_PF_CTRL;
    }

    uint8_t *p = &Event_Management[event].buf[sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];

    //事件类型判断
    if(ctrl_type == INV_CONTROL_POWER_FACTOR)
    {
        PowerFactorEvent_t PowerFactorEvent;

        PowerFactorEvent.start_time = start_time;
        PowerFactorEvent.end_time = end_time;

        for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
        {
            PowerFactorEvent.control_result[i] = ctrl_result[i];
            PowerFactorEvent.inv_values[i] = ctrl_value[i];
            if(1 != PowerFactorEvent.control_result[i])
            {
                if(p[i] < Event_Management[event].Event_Max_Cnt)
                {
                    p[i]++;
                }
            }
        }
        Send_Event_Data_To_Cache_By_Type(event, &PowerFactorEvent);
    }
    else
    {
        PowerControlEvent_t PowerControlEvent;

        PowerControlEvent.start_time = start_time;
        PowerControlEvent.end_time = end_time;
        PowerControlEvent.event_type = event_type;

        for(int i = 0; i < INVERTER_ARCHIVE_MAX_COUNT; ++i)
        {
            PowerControlEvent.control_result[i] = ctrl_result[i];
            PowerControlEvent.control_value[i] = ctrl_value[i];
            if(1 != PowerControlEvent.control_result[i])
            {
                if(p[i] < Event_Management[event].Event_Max_Cnt)
                {
                    p[i]++;
                }
            }
        }
        Send_Event_Data_To_Cache_By_Type(event, &PowerControlEvent);
    }
}

////校时
//void Send_TimeCalibrationEvent_Data(EventTime before_calibration_time, EventTime after_calibration_time)
//{
//    TimeCalibrationEvent_t TimeCalibrationEvent;
//
//    TimeCalibrationEvent.before_calibration = before_calibration_time;
//    TimeCalibrationEvent.after_calibration  = after_calibration_time;
//
//    uint8_t *p = &Event_Management[ETP_PROTO_UNIT_TIME_CALIB].buf[sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];
//
//    (*p)++;
//    Send_Event_Data_To_Cache_By_Type(ETP_PROTO_UNIT_TIME_CALIB, &TimeCalibrationEvent);
//}

////升级
//void Send_UpgradeEvent_Data(uint16_t upgrade_version)
//{
//    uint16_t upgrade_data = upgrade_version;
//
//    uint8_t *p = &Event_Management[ETP_PROTO_UNIT_OTA_UPGRADE].buf[sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];
//    (*p)++;
//    Send_Event_Data_To_Cache_By_Type(ETP_PROTO_UNIT_OTA_UPGRADE, &upgrade_data);
//}

////事件清零
//void Send_EventResetEvent_Data(EventTime event_reset_time)
//{
//    EventTime event_reset_event_data = event_reset_time;
//
//    uint8_t *p = &Event_Management[ETP_PROTO_UNIT_EVENT_CLEAR].buf[sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];
//    (*p)++;
//    Send_Event_Data_To_Cache_By_Type(ETP_PROTO_UNIT_EVENT_CLEAR, &event_reset_event_data);
//}

//电压越限
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

    /* 递增总次数计数器 */
    p = &Event_Management[ETP_VOLT_OVER_LIMIT].buf[
            sizeof(struct Event_Digest_Str) + sizeof(struct Event_Desc_Str)];
    if(*p < Event_Management[ETP_VOLT_OVER_LIMIT].Event_Max_Cnt)
    {
        (*p)++;
    }

    Send_Event_Data_To_Cache_By_Type(ETP_VOLT_OVER_LIMIT, &volt_event);
}





