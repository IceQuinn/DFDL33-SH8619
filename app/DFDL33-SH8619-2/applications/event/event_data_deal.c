#include "event_data_deal.h"
#include "user_ex_flash_mgmt.h"
#include <rtthread.h>
#include <string.h>
#include "drv_ex_flash.h"
#include <time.h>

#define EVENT_DATA_VER              1

struct Event_Management_Str
{
    uint32_t Event_Flash_Addr_A;    // 事件A地址
    uint32_t Event_Flash_Addr_B;    // 事件B地址
    uint32_t Event_Max_Cnt;         // 事件最大数量
    
    uint8_t  buf[4096]; // 事件数据缓存区
//    struct   Events_Str Events;     // 事件结构
    char     *Event_Name;           // 事件描述
};


struct Event_Management_Str Event_Management[EVT_CLASS_MAX_] =
{
    {.Event_Flash_Addr_A = FLASH_PV_INV_ONOFF_CTRL_EVENT_A_ADDR,        .Event_Flash_Addr_B = FLASH_PV_INV_ONOFF_CTRL_EVENT_B_ADDR,         .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_ONOFF_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_P_CTRL_EVENT_A_ADDR,            .Event_Flash_Addr_B = FLASH_PV_INV_P_CTRL_EVENT_B_ADDR,             .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_P_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_Q_CTRL_EVENT_A_ADDR,            .Event_Flash_Addr_B = FLASH_PV_INV_Q_CTRL_EVENT_B_ADDR,             .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_Q_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_PF_CTRL_EVENT_A_ADDR,           .Event_Flash_Addr_B = FLASH_PV_INV_PF_CTRL_EVENT_B_ADDR,            .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_PF_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_P_PERIOD_CTRL_EVENT_A_ADDR,     .Event_Flash_Addr_B = FLASH_PV_INV_P_PERIOD_CTRL_EVENT_B_ADDR,      .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_P_PERIOD_CTRL_EVENT"},
    {.Event_Flash_Addr_A = FLASH_VOLT_OVER_LIMIT_EVENT_A_ADDR,          .Event_Flash_Addr_B = FLASH_VOLT_OVER_LIMIT_EVENT_B_ADDR,           .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "VOLT_OVER_LIMIT_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PV_INV_COMM_ERR_EVENT_A_ADDR,          .Event_Flash_Addr_B = FLASH_PV_INV_COMM_ERR_EVENT_B_ADDR,           .Event_Max_Cnt = EVENT_MAX_DATE_NUM_50,  .buf = {0}, .Event_Name = "PV_INV_COMM_ERR_EVENT"},
    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_EVENT_CLEAR_EVENT_A_ADDR,   .Event_Flash_Addr_B = FLASH_PROTO_UNIT_EVENT_CLEAR_EVENT_B_ADDR,    .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Event Reset Event"},
    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_OTA_UPGRADE_EVENT_A_ADDR,   .Event_Flash_Addr_B = FLASH_PROTO_UNIT_OTA_UPGRADE_EVENT_B_ADDR,    .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Upgrading Event"},
    {.Event_Flash_Addr_A = FLASH_PROTO_UNIT_TIME_CALIB_EVENT_A_ADDR,    .Event_Flash_Addr_B = FLASH_PROTO_UNIT_TIME_CALIB_EVENT_A_ADDR,     .Event_Max_Cnt = EVENT_MAX_DATE_NUM_100, .buf = {0}, .Event_Name = "Time Calibration Event"},
};

uint8_t Events_Cache_Date[EVENTS_CACHE_NUM_MAX][ONE_EVENT_RECORD_SIZE];// 事件
/* 消息队列控制块 */
static struct rt_messagequeue Event_Data_mq;


int32_t Send_Event_Data_To_Cache_By_Type(uint16_t event_class, void *p_act_val)
{
    uint8_t Events_Cache_Date[ONE_EVENT_RECORD_SIZE] = {0};
    Events_Cache_Date[0] = event_class;
    memcpy(&Events_Cache_Date[1], p_act_val, ONE_EVENT_RECORD_SIZE - 1);

    if(RT_EOK != rt_mq_send(&Event_Data_mq, &Events_Cache_Date, sizeof(Events_Cache_Date)))
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
                        RT_IPC_FLAG_PRIO);              /* 如果有多个线程等待，优先级大小的方法分配消息 */

    if (result != RT_EOK)
    {
        rt_kprintf("init Event message queue failed.\n");
        return -RT_ERROR;
    }
    return RT_EOK;
}




// int32_t Show_Event(uint8_t Events_Cache_Date *p_Event_Date, uint16_t idx, uint8_t simple_en)
// {
//     struct tm t = {0};

//     localtime_r((const time_t*)&p_Event_Date->times, &t);
//     for(uint16_t i=0; i<Event_Table_Size; ++i)
//     {
//         if(p_Event_Date->event_id == event_desc[i].evt_id)
//         {
//             p_Event_Date->event_type = event_desc[i].evt_class;
//             LOG_I("[%03d] %d-%02d-%02d %02d:%02d:%02d.%03d : <%s> %s", idx,
//                     t.tm_year + 1900, t.tm_mon+1, t.tm_mday,
//                     t.tm_hour, t.tm_min, t.tm_sec,
//                     p_Event_Date->ms, event_desc[i].evt_desc, p_Event_Date->val?"动作":"复归");

//             if(1 == simple_en){
//                 show_evt_act_val(p_Event_Date->val_type, p_Event_Date->act_val_buf);
//             }
//             return RT_EOK;
//         }
//     }
//     LOG_I("Find Event Failed event_class = %d, event_type = %d", p_Event_Date->event_id, p_Event_Date->event_type);
//     return -RT_ERROR;
// }


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
    else if(event_class == ETP_PROTO_UNIT_EVENT_CLEAR){
        size = sizeof(EventTime);
        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PROTO_UNIT_OTA_UPGRADE){
        size = sizeof(uint16_t);
        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
    }
    else if(event_class == ETP_PROTO_UNIT_TIME_CALIB){
        size = sizeof(TimeCalibrationEvent_t);
        p_save_addr = &p_buf[2 + tailIdx * size]; // 事件数据缓存区
    }


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
    rt_memset(&Event_Management[event_class].buf, 0, sizeof(Event_Management[event_class].buf));
    rt_kprintf("%s has been Clear\n", Event_Management[event_class].Event_Name);
    Save_Event_Data_To_Flash(event_class);
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
    uint8_t Events_Cache_Date[ONE_EVENT_RECORD_SIZE] = {0};
    uint8_t Event_Save_Class_Bits_Flg = 0;
    rt_thread_mdelay(4000);
    while(1)
    {
        Event_Save_Class_Bits_Flg = 0;
        if (rt_mq_recv(&Event_Data_mq, Events_Cache_Date, sizeof(Events_Cache_Date), RT_WAITING_FOREVER) == RT_EOK)
        {
//            Show_Event(&Events_Cache_Date, 0, 1);
            // if(is_power_down() && !is_power_down_legal_work()){
            //     LOG_W("Power Down! Do not save new event");
            //     continue;
            // }

            Save_Event_Data(Events_Cache_Date);
            // 记录需要保存的事件类型
            Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Events_Cache_Date[0]);

            // 尝试取出队列里剩余消息（非阻塞）
            while (rt_mq_recv(&Event_Data_mq, Events_Cache_Date, sizeof(Events_Cache_Date), 0) == RT_EOK)
            {
//                Show_Event(&Events_Cache_Date, 0, 1);
                Save_Event_Data(Events_Cache_Date);
                // 记录需要保存的事件类型
                Event_Save_Class_Bits_Flg = Event_Save_Class_Bits_Flg | (1 << Events_Cache_Date[0]);
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
