#ifndef __EVENT_DATA_DEAL_H__
#define __EVENT_DATA_DEAL_H__

#include <stdint.h>
#include "AB_check.h"

#define EVENTS_CACHE_NUM_MAX        20
#define ONE_EVENT_RECORD_SIZE       (128) // 事件记录大小

//#define EXTFLASH_PAGE_SIZE          (4096)

// 开关机事项
//#define FLASH_SWITCH_EVENT_A_ADDR   (144    * EXTFLASH_PAGE_SIZE)   // 4个扇区
//#define FLASH_SWITCH_EVENT_B_ADDR   (148    * EXTFLASH_PAGE_SIZE)   // 4个扇区
//// 功率控制事项
//#define FLASH_POWER_CONTROL_EVENT_A_ADDR      (152    * EXTFLASH_PAGE_SIZE)   // 4个扇区
//#define FLASH_POWER_CONTROL_EVENT_B_ADDR      (156    * EXTFLASH_PAGE_SIZE)   // 4个扇区
//// 校时事项
//#define FLASH_TIME_CALIBRATION_EVENT_A_ADDR      (160    * EXTFLASH_PAGE_SIZE)   // 4个扇区
//#define FLASH_TIME_CALIBRATION_EVENT_B_ADDR      (164    * EXTFLASH_PAGE_SIZE)   // 4个扇区


#define EVENT_MAX_INVERTER_NUM  12
#define EVENT_MAX_DATE_NUM_50   50
#define EVENT_MAX_DATE_NUM_100  100

struct Event_Digest_Str
{
    rcd_head hdr;
    uint16_t eventCnt;  // flash中一共有多少条记录
    uint16_t headIdx;   // flash记录中，第一条记录的启动偏移
    uint16_t tailIdx;   // flash记录中，最后一条记录的启动偏移
};

struct Event_Desc_Str
{
    uint16_t evt_class;   //事件序号
    //
    // uint32_t times;         //事件触发时间
    // uint16_t ms;            //事件触发时间-ms
};

// 事件时间：YYMMDDhhmmss
typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} EventTime;

/* 1、开关机控制事件详细记录 (DI1=01) */
typedef struct {
    EventTime start_time;
    EventTime end_time;
    uint8_t   event_type[EVENT_MAX_INVERTER_NUM];     // 00开机, 01关机
    uint8_t   control_result[EVENT_MAX_INVERTER_NUM]; // 调控结果，左起逆变器12~1
}SwitchEvent_t;

/* 2、功率控制事件 (有功/无功/时段) */
typedef struct {
    EventTime start_time;                               //开始时间
    EventTime end_time;                                 //结束时间
    uint8_t   event_type;                               //事件类型（功率值调节/百分比调节）
    uint8_t   control_result[EVENT_MAX_INVERTER_NUM];   //调控结果
    int32_t   control_value[EVENT_MAX_INVERTER_NUM];    //调控值
}PowerControlEvent_t;

/* 3、功率因数控制事件详细记录 (DI1=07) */
typedef struct {
    EventTime start_time;
    EventTime end_time;
    uint8_t   control_result[EVENT_MAX_INVERTER_NUM];
    int16_t   inv_values[EVENT_MAX_INVERTER_NUM];
}PowerFactorEvent_t;

/* 4、电压越限事件详细记录 (DI1=11) */
typedef struct {
    EventTime start_time;
    EventTime end_time;
    uint16_t  over_limit_vol_a;
    uint16_t  over_limit_vol_b;
    uint16_t  over_limit_vol_c;
    uint16_t  recover_vol_a;
    uint16_t  recover_vol_b;
    uint16_t  recover_vol_c;
}VoltageOverLimit_t;

/* 5、通信异常事件详细记录 (DI1=13) */
typedef struct {
    EventTime start_time;
    EventTime end_time;
    uint8_t   inv_id;
}CommErrEvent_t;

/* 6、校时事件详细记录 */
typedef struct {
    EventTime before_calibration;
    EventTime after_calibration;
}TimeCalibrationEvent_t;


/* ---- 各类事件数据库 ---- */
//逆变器开关机控制事件(1826字节)
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t event_data_cnt[EVENT_MAX_INVERTER_NUM];
    SwitchEvent_t switch_event_record[EVENT_MAX_DATE_NUM_50];
} SwitchEvent;

//逆变器功率控制事件(3676字节)
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t event_data_cnt[EVENT_MAX_INVERTER_NUM];
    PowerControlEvent_t power_control_event_record[EVENT_MAX_DATE_NUM_50];
} PowerControlEvent;

//逆变器功率因数控制事件(2438字节)
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t pf_event_cnt[EVENT_MAX_INVERTER_NUM];
    PowerFactorEvent_t pf_event_data[EVENT_MAX_DATE_NUM_50];
} PowerFactorEvent;

//电压越限事件(1216字节)
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t voltage_event_cnt;
    VoltageOverLimit_t voltage_event_data[EVENT_MAX_DATE_NUM_50];
} VoltageOverLimit;

//逆变器通信异常事件(688字节)
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t comm_err_event_cnt[EVENT_MAX_INVERTER_NUM];
    CommErrEvent_t comm_err_event_data[EVENT_MAX_DATE_NUM_50];
} CommErrEvent;

//协议转换单元事件清零事件
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t event_reset_event_cnt;
    EventTime event_reset_event_data[EVENT_MAX_DATE_NUM_100];
}EventResetEvent;

//协议转换单元程序升级事件
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t upgrading_event_cnt;
    uint16_t upgrading_event_data[EVENT_MAX_DATE_NUM_100];
}UpgradingEvent;

//协议转换单元校时事件
typedef struct {
    struct Event_Digest_Str event_digest;
    struct Event_Desc_Str event_desc;
    uint16_t calibration_time_cnt;
    TimeCalibrationEvent_t calibration_time_data[EVENT_MAX_DATE_NUM_100];
}TimeCalibrationEvent;



enum Event_Class
{
    ETP_PV_INV_ONOFF_CTRL,              // 光伏逆变器开关机控制事件
    ETP_PV_INV_P_CTRL,                  // 光伏逆变器有功功率控制事件
    ETP_PV_INV_Q_CTRL,                  // 光伏逆变器无功功率控制事件
    ETP_PV_INV_PF_CTRL,                 // 光伏逆变器功率因数控制事件
    ETP_PV_INV_P_PERIOD_CTRL,           // 光伏逆变器有功功率时段控制事件
    ETP_VOLT_OVER_LIMIT,                // 电压越限事件
    ETP_PV_INV_COMM_ERR,                // 光伏逆变器通讯异常事件
    ETP_PROTO_UNIT_EVENT_CLEAR,         // 协议转换单元事件清零事件
    ETP_PROTO_UNIT_OTA_UPGRADE,         // 协议转换单元软件在线升级事件
    ETP_PROTO_UNIT_TIME_CALIB,          // 协议转换单元校时事件

    EVT_CLASS_MAX_,
};

void Get_EventTime(EventTime *evt_time);

void Event_Data_Init(void);

int32_t Send_Event_Data_To_Cache_By_Type(uint16_t event_class, void *p_act_val);

void Event_Data_Deal_Loop(void *parameter);



#endif
