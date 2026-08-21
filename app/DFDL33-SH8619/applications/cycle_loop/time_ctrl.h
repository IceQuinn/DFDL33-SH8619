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

#define TIME_CTRL_PERIOD_COUNT 2U

/* 命令时间使用本地RTC时间，年份为完整四位年份。 */
typedef struct Time_Ctrl_DateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} Time_Ctrl_DateTime_t;

typedef enum Time_Ctrl_Mode
{
    TIME_CTRL_ACTIVE_POWER_VALUE = 0,
    TIME_CTRL_ACTIVE_POWER_PERCENT = 1
} Time_Ctrl_Mode_t;

typedef struct Time_Ctrl_Period
{
    Time_Ctrl_DateTime_t start;
    Time_Ctrl_DateTime_t end;
    Time_Ctrl_Mode_t mode;
    int32_t value; /* 与协议decimal_places一致的定点整数。 */
} Time_Ctrl_Period_t;

typedef struct Time_Ctrl_Command
{
    Time_Ctrl_Period_t periods[TIME_CTRL_PERIOD_COUNT];
} Time_Ctrl_Command_t;

/* Set接口的应答码可直接转换成上行协议回复。 */
typedef enum Time_Ctrl_Result
{
    TIME_CTRL_RESULT_OK = 0,
    TIME_CTRL_RESULT_INVALID_PARAMETER,
    TIME_CTRL_RESULT_INVALID_TIME,
    TIME_CTRL_RESULT_CROSS_DAY,
    TIME_CTRL_RESULT_INVALID_RANGE,
    TIME_CTRL_RESULT_OVERLAP,
    TIME_CTRL_RESULT_BUSY,
    TIME_CTRL_RESULT_THREAD_ERROR
} Time_Ctrl_Result_t;

Time_Ctrl_Result_t Time_Ctrl_Set(const Time_Ctrl_Command_t *command);
Time_Ctrl_Result_t Time_Ctrl_Stop(void);
Time_Ctrl_Result_t Time_Ctrl_Init(void);
const char *Time_Ctrl_Result_Text(Time_Ctrl_Result_t result);

void time_ctrl_thread_entry(void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_TIME_CTRL_H_ */
