/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APPLICATIONS_CYCLE_LOOP_TIME_CTRL_H_
#define APPLICATIONS_CYCLE_LOOP_TIME_CTRL_H_

#include <stdint.h>
#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 每台逆变器最多配置两个互不重叠的控制时段。 */
#define TIME_CTRL_PERIOD_COUNT 2U

/* 年月日时分秒形式的设备本地RTC时间。 */
typedef struct Time_Ctrl_DateTime
{
    uint16_t year;  /* 完整四位年份，当前支持1970～2099。 */
    uint8_t month;  /* 月，范围1～12。 */
    uint8_t day;    /* 日，范围取决于年月。 */
    uint8_t hour;   /* 时，范围0～23。 */
    uint8_t minute; /* 分，范围0～59。 */
    uint8_t second; /* 秒，范围0～59。 */
} Time_Ctrl_DateTime_t;

/* 当前支持的两种有功功率控制方式。 */
typedef enum Time_Ctrl_Mode
{
    TIME_CTRL_ACTIVE_POWER_VALUE = 0,  /* 有功功率数值控制。 */
    TIME_CTRL_ACTIVE_POWER_PERCENT = 1 /* 有功功率百分比控制。 */
} Time_Ctrl_Mode_t;

/* 一次有功功率调节的开始时间、结束时间、方式和数值。 */
typedef struct Time_Ctrl_Period
{
    Time_Ctrl_DateTime_t start; /* 调节开始时间，包含该时刻。 */
    Time_Ctrl_DateTime_t end;   /* 调节结束时间，不包含该时刻。 */
    Time_Ctrl_Mode_t mode;      /* 有功数值或有功百分比控制。 */
    int32_t value;              /* 与目标协议decimal_places一致的定点整数。 */
} Time_Ctrl_Period_t;

/* 单个逆变器档案的完整时段控制命令。 */
typedef struct Time_Ctrl_Command
{
    uint8_t archive_index; /* 目标档案槽位下标，范围0～11。 */
    Time_Ctrl_Period_t periods[TIME_CTRL_PERIOD_COUNT]; /* 两个控制时段。 */
} Time_Ctrl_Command_t;

/* 时段命令受理结果，可直接转换为上行协议回复。 */
typedef enum Time_Ctrl_Result
{
    TIME_CTRL_RESULT_OK = 0,           /* 命令已经受理。 */
    TIME_CTRL_RESULT_INVALID_PARAMETER, /* 控制方式等普通参数无效。 */
    TIME_CTRL_RESULT_INVALID_TIME,     /* 日期或时间字段无效。 */
    TIME_CTRL_RESULT_CROSS_DAY,        /* 某个控制时段跨天。 */
    TIME_CTRL_RESULT_INVALID_RANGE,    /* 开始时间不早于结束时间。 */
    TIME_CTRL_RESULT_OVERLAP,          /* 两个控制时段存在重叠。 */
    TIME_CTRL_RESULT_ARCHIVE_INVALID,  /* 档案下标越界或档案无效。 */
    TIME_CTRL_RESULT_UNSUPPORTED,      /* 目标协议不支持调控或额定恢复。 */
    TIME_CTRL_RESULT_INVALID_VALUE,    /* 控制值超出协议数据类型或百分比范围。 */
    TIME_CTRL_RESULT_THREAD_ERROR      /* 时段控制线程尚未完成初始化。 */
} Time_Ctrl_Result_t;

/*
 * 按值受理一台逆变器的时段命令。
 * 旧命令未开始时直接更新；旧命令执行中时先排队恢复额定功率，再执行新命令。
 */
Time_Ctrl_Result_t Time_Ctrl_Set(Time_Ctrl_Command_t command);

/* 停止指定档案的时段控制；正在调控时会先排队恢复额定功率。 */
Time_Ctrl_Result_t Time_Ctrl_Stop_Archive(uint8_t archive_index);

/* 停止全部12个档案的时段控制。 */
Time_Ctrl_Result_t Time_Ctrl_Stop(void);

/* 将时段控制结果码转换成固定英文说明。 */
const char *Time_Ctrl_Result_Text(Time_Ctrl_Result_t result);

/* 打印全部有效档案中已经启用的时段控制，没有时段控制的档案不输出。 */
void Time_Ctrl_Print_All(void);

/* 线程表使用的时段控制入口，应用层只能创建一个该线程实例。 */
void time_ctrl_thread_entry(void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_TIME_CTRL_H_ */
