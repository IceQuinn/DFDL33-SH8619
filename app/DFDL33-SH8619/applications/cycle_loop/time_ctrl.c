/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "time_ctrl.h"

#include <limits.h>
#include <time.h>

#include <rthw.h>

#include "inv_data.h"
#include "inverter_archive.h"
#include "inverter_protocol_library.h"
#include "user_rtc.h"

#define TIME_CTRL_THREAD_STACK_SIZE     2048U
#define TIME_CTRL_THREAD_PRIORITY          18U
#define TIME_CTRL_THREAD_SLICE              15U
#define TIME_CTRL_POLL_MS                 200U
#define TIME_CTRL_RESTORE_PERCENT          100
#define TIME_CTRL_NO_ACTIVE_PERIOD          -1

/* FinSH无参数测试命令使用的默认相对时间和控制值。 */
#define TIME_CTRL_TEST_PERIOD1_START_SEC     5
#define TIME_CTRL_TEST_PERIOD1_END_SEC      35
#define TIME_CTRL_TEST_PERIOD2_START_SEC    45
#define TIME_CTRL_TEST_PERIOD2_END_SEC      75
#define TIME_CTRL_TEST_PERCENT_VALUE        80
#define TIME_CTRL_TEST_POWER_VALUE        3000

typedef enum Time_Ctrl_Action
{
    TIME_CTRL_ACTION_NONE = 0,
    TIME_CTRL_ACTION_START,
    TIME_CTRL_ACTION_RESTORE
} Time_Ctrl_Action_t;

/* Set/Stop只在关中断的短临界区写入该邮箱，调度线程持有运行时副本。 */
static Time_Ctrl_Command_t g_time_ctrl_command;
static volatile uint32_t g_time_ctrl_generation;
static volatile rt_bool_t g_time_ctrl_enabled;
static volatile rt_bool_t g_time_ctrl_active;
static rt_thread_t g_time_ctrl_thread = RT_NULL;
static uint32_t g_time_ctrl_request_id;

static rt_bool_t time_ctrl_is_leap_year(uint16_t year)
{
    return (((year % 4U) == 0U) &&
            (((year % 100U) != 0U) || ((year % 400U) == 0U))) ? RT_TRUE : RT_FALSE;
}

static uint8_t time_ctrl_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U, 28U, 31U, 30U, 31U, 30U,
                                   31U, 31U, 30U, 31U, 30U, 31U};

    if((month == 0U) || (month > 12U)) {
        return 0U;
    }
    if((month == 2U) && (time_ctrl_is_leap_year(year) == RT_TRUE)) {
        return 29U;
    }
    return days[month - 1U];
}

static rt_bool_t time_ctrl_datetime_valid(const Time_Ctrl_DateTime_t *value)
{
    if((value == RT_NULL) || (value->year < 1970U) || (value->year > 2099U) ||
       (value->month < 1U) || (value->month > 12U) ||
       (value->day < 1U) || (value->day > time_ctrl_days_in_month(value->year, value->month)) ||
       (value->hour > 23U) || (value->minute > 59U) || (value->second > 59U)) {
        return RT_FALSE;
    }
    return RT_TRUE;
}

static rt_bool_t time_ctrl_same_day(const Time_Ctrl_DateTime_t *left,
                                    const Time_Ctrl_DateTime_t *right)
{
    return ((left->year == right->year) && (left->month == right->month) &&
            (left->day == right->day)) ? RT_TRUE : RT_FALSE;
}

/* 参数先经过严格日历检查，mktime只负责按设备本地时区换算time_t。 */
static time_t time_ctrl_to_time(const Time_Ctrl_DateTime_t *value)
{
    struct tm local_time;

    rt_memset(&local_time, 0, sizeof(local_time));
    local_time.tm_year = (int)value->year - 1900;
    local_time.tm_mon = (int)value->month - 1;
    local_time.tm_mday = value->day;
    local_time.tm_hour = value->hour;
    local_time.tm_min = value->minute;
    local_time.tm_sec = value->second;
    local_time.tm_isdst = -1;
    return mktime(&local_time);
}

static Time_Ctrl_Result_t time_ctrl_validate(const Time_Ctrl_Command_t *command,
                                             time_t starts[TIME_CTRL_PERIOD_COUNT],
                                             time_t ends[TIME_CTRL_PERIOD_COUNT])
{
    uint8_t index;

    if((command == RT_NULL) || (starts == RT_NULL) || (ends == RT_NULL)) {
        return TIME_CTRL_RESULT_INVALID_PARAMETER;
    }

    for(index = 0U; index < TIME_CTRL_PERIOD_COUNT; ++index) {
        const Time_Ctrl_Period_t *period = &command->periods[index];

        if((period->mode != TIME_CTRL_ACTIVE_POWER_VALUE) &&
           (period->mode != TIME_CTRL_ACTIVE_POWER_PERCENT)) {
            return TIME_CTRL_RESULT_INVALID_PARAMETER;
        }
        if((time_ctrl_datetime_valid(&period->start) == RT_FALSE) ||
           (time_ctrl_datetime_valid(&period->end) == RT_FALSE)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        if(time_ctrl_same_day(&period->start, &period->end) == RT_FALSE) {
            return TIME_CTRL_RESULT_CROSS_DAY;
        }

        starts[index] = time_ctrl_to_time(&period->start);
        ends[index] = time_ctrl_to_time(&period->end);
        if((starts[index] == (time_t)-1) || (ends[index] == (time_t)-1)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        if(starts[index] >= ends[index]) {
            return TIME_CTRL_RESULT_INVALID_RANGE;
        }
    }

    /* 使用[start,end)区间，相邻时段首尾相接不算重叠。 */
    if((starts[0] < ends[1]) && (starts[1] < ends[0])) {
        return TIME_CTRL_RESULT_OVERLAP;
    }
    return TIME_CTRL_RESULT_OK;
}

const char *Time_Ctrl_Result_Text(Time_Ctrl_Result_t result)
{
    static const char *texts[] = {
        "ok", "invalid parameter", "invalid time", "cross-day period",
        "start must be before end", "periods overlap", "control is active",
        "thread create failed"
    };

    if((uint32_t)result >= (sizeof(texts) / sizeof(texts[0]))) {
        return "unknown error";
    }
    return texts[result];
}

Time_Ctrl_Result_t Time_Ctrl_Set(const Time_Ctrl_Command_t *command)
{
    time_t starts[TIME_CTRL_PERIOD_COUNT];
    time_t ends[TIME_CTRL_PERIOD_COUNT];
    Time_Ctrl_Result_t result;
    rt_base_t level;

    result = time_ctrl_validate(command, starts, ends);
    if(result != TIME_CTRL_RESULT_OK) {
        return result;
    }

    level = rt_hw_interrupt_disable();
    if(g_time_ctrl_active == RT_TRUE) {
        rt_hw_interrupt_enable(level);
        return TIME_CTRL_RESULT_BUSY;
    }
    g_time_ctrl_command = *command;
    g_time_ctrl_enabled = RT_TRUE;
    ++g_time_ctrl_generation;
    rt_hw_interrupt_enable(level);
    return TIME_CTRL_RESULT_OK;
}

Time_Ctrl_Result_t Time_Ctrl_Stop(void)
{
    rt_base_t level = rt_hw_interrupt_disable();

    g_time_ctrl_enabled = RT_FALSE;
    ++g_time_ctrl_generation;
    rt_hw_interrupt_enable(level);
    return TIME_CTRL_RESULT_OK;
}

static uint16_t time_ctrl_valid_archive_mask(void)
{
    uint16_t mask = 0U;
    uint8_t index;

    for(index = 0U; index < INVERTER_ARCHIVE_MAX_COUNT; ++index) {
        if(g_inv_archive_lib.valid[index] == INVERTER_ARCHIVE_VALID) {
            mask |= (uint16_t)(1U << index);
        }
    }
    return mask;
}

/* 额定恢复使用有功百分比100%，并按每台协议的小数位生成定点值。 */
static rt_bool_t time_ctrl_restore_value(uint8_t archive_index, int32_t *value)
{
    const Inv_Proto_t *protocol;
    Inv_CtrlRegBlk_t ctrl;
    int32_t scale = 1;
    uint8_t index;

    if(value == RT_NULL) {
        return RT_FALSE;
    }
    protocol = Inv_Archive_Get_Protocol(archive_index);
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    /* 协议结构为1字节对齐，复制后读取位域，避免非对齐访问。 */
    rt_memcpy(&ctrl, &protocol->ctrl.active_pwr_pct_ctrl, sizeof(ctrl));
    for(index = 0U; index < ctrl.decimal_places; ++index) {
        if(scale > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scale *= 10;
    }
    if(scale > (INT32_MAX / TIME_CTRL_RESTORE_PERCENT)) {
        return RT_FALSE;
    }
    *value = TIME_CTRL_RESTORE_PERCENT * scale;
    return RT_TRUE;
}

/* 队列满或周期抄读尚未初始化时保留bit，下一轮继续提交。 */
static void time_ctrl_submit_pending(uint16_t *pending_mask,
                                     uint16_t *affected_mask,
                                     Time_Ctrl_Action_t action,
                                     const Time_Ctrl_Period_t *period)
{
    uint8_t archive_index;

    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        uint16_t bit = (uint16_t)(1U << archive_index);
        Inv_Control_Request_t request;

        if((*pending_mask & bit) == 0U) {
            continue;
        }

        rt_memset(&request, 0, sizeof(request));
        request.request_id = ++g_time_ctrl_request_id;
        request.archive_index = archive_index;
        if(action == TIME_CTRL_ACTION_RESTORE) {
            request.type = INV_CONTROL_ACTIVE_POWER_PERCENT;
            if(time_ctrl_restore_value(archive_index, &request.value) == RT_FALSE) {
                *pending_mask &= (uint16_t)(~bit);
                rt_kprintf("%s time control archive[%d] restore value unavailable\n",
                           get_char_time(), archive_index + 1U);
                continue;
            }
        }
        else {
            request.type = (period->mode == TIME_CTRL_ACTIVE_POWER_PERCENT) ?
                           INV_CONTROL_ACTIVE_POWER_PERCENT : INV_CONTROL_ACTIVE_POWER;
            request.value = period->value;
        }

        if(Inv_Control_Submit(&request) == RT_EOK) {
            *pending_mask &= (uint16_t)(~bit);
            if(action == TIME_CTRL_ACTION_START) {
                *affected_mask |= bit;
            }
        }
        else if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) {
            /* 档案在命令执行期间失效时不再无限重试该槽位。 */
            *pending_mask &= (uint16_t)(~bit);
        }
    }
}

void time_ctrl_thread_entry(void *parameter)
{
    Time_Ctrl_Command_t command;
    time_t starts[TIME_CTRL_PERIOD_COUNT] = {0};
    time_t ends[TIME_CTRL_PERIOD_COUNT] = {0};
    uint32_t generation = 0U;
    uint16_t pending_mask = 0U;
    uint16_t affected_mask = 0U;
    uint8_t done_mask = 0U;
    int8_t active_period = TIME_CTRL_NO_ACTIVE_PERIOD;
    int8_t action_period = TIME_CTRL_NO_ACTIVE_PERIOD;
    Time_Ctrl_Action_t action = TIME_CTRL_ACTION_NONE;

    RT_UNUSED(parameter);
    rt_memset(&command, 0, sizeof(command));

    while(1) {
        time_t now = time(RT_NULL); /* 轮询时间来自C库time()和RTC。 */
        rt_bool_t enabled;
        uint32_t current_generation;
        rt_base_t level;
        uint8_t index;

        level = rt_hw_interrupt_disable();
        current_generation = g_time_ctrl_generation;
        enabled = g_time_ctrl_enabled;
        if(current_generation != generation) {
            command = g_time_ctrl_command;
        }
        rt_hw_interrupt_enable(level);

        if(current_generation != generation) {
            /* 替换/停止时，已经提交过调节的设备必须先恢复额定输出。 */
            if(affected_mask != 0U) {
                pending_mask = affected_mask;
                action = TIME_CTRL_ACTION_RESTORE;
                /* 这是旧命令的清理动作，不能把新命令的同下标时段标成已完成。 */
                action_period = TIME_CTRL_NO_ACTIVE_PERIOD;
            }
            else {
                action = TIME_CTRL_ACTION_NONE;
                pending_mask = 0U;
                g_time_ctrl_active = RT_FALSE;
            }
            active_period = TIME_CTRL_NO_ACTIVE_PERIOD;
            done_mask = 0U;
            generation = current_generation;
            if(enabled == RT_TRUE) {
                (void)time_ctrl_validate(&command, starts, ends);
            }
        }

        /* 排队耗时跨过结束点时立即停止继续下发，并恢复已经受影响的设备。 */
        if((action == TIME_CTRL_ACTION_START) && (action_period >= 0) &&
           (now >= ends[(uint8_t)action_period])) {
            done_mask |= (uint8_t)(1U << (uint8_t)action_period);
            pending_mask = affected_mask;
            action = (pending_mask != 0U) ? TIME_CTRL_ACTION_RESTORE : TIME_CTRL_ACTION_NONE;
            action_period = TIME_CTRL_NO_ACTIVE_PERIOD;
            if(action == TIME_CTRL_ACTION_NONE) {
                g_time_ctrl_active = RT_FALSE;
            }
        }

        if(action != TIME_CTRL_ACTION_NONE) {
            const Time_Ctrl_Period_t *period = (action_period >= 0) ?
                                               &command.periods[(uint8_t)action_period] : RT_NULL;
            time_ctrl_submit_pending(&pending_mask, &affected_mask, action, period);
            if(pending_mask == 0U) {
                if(action == TIME_CTRL_ACTION_START) {
                    active_period = action_period;
                    g_time_ctrl_active = RT_TRUE;
                    rt_kprintf("%s time control period[%d] started\n",
                               get_char_time(), active_period + 1);
                }
                else {
                    affected_mask = 0U;
                    g_time_ctrl_active = RT_FALSE;
                    if(action_period >= 0) {
                        done_mask |= (uint8_t)(1U << (uint8_t)action_period);
                    }
                    rt_kprintf("%s time control restored rated active power\n", get_char_time());
                }
                action = TIME_CTRL_ACTION_NONE;
                action_period = TIME_CTRL_NO_ACTIVE_PERIOD;
            }
        }

        /* Stop命令只允许完成恢复，不再启动新时段。 */
        if((enabled == RT_TRUE) && (action == TIME_CTRL_ACTION_NONE)) {
            if((active_period >= 0) && (now >= ends[(uint8_t)active_period])) {
                pending_mask = affected_mask;
                action = TIME_CTRL_ACTION_RESTORE;
                action_period = active_period;
                active_period = TIME_CTRL_NO_ACTIVE_PERIOD;
            }
            else if(active_period < 0) {
                for(index = 0U; index < TIME_CTRL_PERIOD_COUNT; ++index) {
                    uint8_t bit = (uint8_t)(1U << index);

                    if((done_mask & bit) != 0U) {
                        continue;
                    }
                    if(now >= ends[index]) {
                        done_mask |= bit;
                    }
                    else if((now >= starts[index]) && (now < ends[index])) {
                        pending_mask = time_ctrl_valid_archive_mask();
                        affected_mask = 0U;
                        action = TIME_CTRL_ACTION_START;
                        action_period = (int8_t)index;
                        if(pending_mask == 0U) {
                            done_mask |= bit;
                            action = TIME_CTRL_ACTION_NONE;
                            rt_kprintf("%s time control period[%d] has no valid archive\n",
                                       get_char_time(), index + 1U);
                        }
                        else {
                            /* 包含正在排队的调节，防止新Set命令覆盖半完成的批量控制。 */
                            g_time_ctrl_active = RT_TRUE;
                        }
                        break;
                    }
                }
            }
        }

        rt_thread_mdelay(TIME_CTRL_POLL_MS);
    }
}

Time_Ctrl_Result_t Time_Ctrl_Init(void)
{
    if(g_time_ctrl_thread != RT_NULL) {
        return TIME_CTRL_RESULT_OK;
    }

    g_time_ctrl_thread = rt_thread_create("time_ctrl", time_ctrl_thread_entry, RT_NULL,
                                          TIME_CTRL_THREAD_STACK_SIZE,
                                          TIME_CTRL_THREAD_PRIORITY,
                                          TIME_CTRL_THREAD_SLICE);
    if(g_time_ctrl_thread == RT_NULL) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }
    if(rt_thread_startup(g_time_ctrl_thread) != RT_EOK) {
        rt_thread_delete(g_time_ctrl_thread);
        g_time_ctrl_thread = RT_NULL;
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }
    return TIME_CTRL_RESULT_OK;
}

static rt_bool_t time_ctrl_test_datetime(time_t value, Time_Ctrl_DateTime_t *datetime)
{
    struct tm *local_time;

    if(datetime == RT_NULL) {
        return RT_FALSE;
    }

    local_time = localtime(&value);
    if(local_time == RT_NULL) {
        return RT_FALSE;
    }

    datetime->year = (uint16_t)(local_time->tm_year + 1900);
    datetime->month = (uint8_t)(local_time->tm_mon + 1);
    datetime->day = (uint8_t)local_time->tm_mday;
    datetime->hour = (uint8_t)local_time->tm_hour;
    datetime->minute = (uint8_t)local_time->tm_min;
    datetime->second = (uint8_t)local_time->tm_sec;
    return RT_TRUE;
}

/* 无参数测试命令自动使用当前RTC时间生成两个不重叠时段。 */
static Time_Ctrl_Result_t time_ctrl_test_set_default(void)
{
    Time_Ctrl_Command_t command;
    time_t now = time(RT_NULL);

    if(now == (time_t)-1) {
        return TIME_CTRL_RESULT_INVALID_TIME;
    }

    rt_memset(&command, 0, sizeof(command));
    if((time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD1_START_SEC,
                                &command.periods[0].start) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD1_END_SEC,
                                &command.periods[0].end) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD2_START_SEC,
                                &command.periods[1].start) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD2_END_SEC,
                                &command.periods[1].end) == RT_FALSE)) {
        return TIME_CTRL_RESULT_INVALID_TIME;
    }

    command.periods[0].mode = TIME_CTRL_ACTIVE_POWER_PERCENT;
    command.periods[0].value = TIME_CTRL_TEST_PERCENT_VALUE;
    command.periods[1].mode = TIME_CTRL_ACTIVE_POWER_VALUE;
    command.periods[1].value = TIME_CTRL_TEST_POWER_VALUE;

    rt_kprintf("%s test period[1] %04d-%02d-%02d %02d:%02d:%02d - %02d:%02d:%02d, percent[%d]\n",
               get_char_time(),
               command.periods[0].start.year, command.periods[0].start.month,
               command.periods[0].start.day, command.periods[0].start.hour,
               command.periods[0].start.minute, command.periods[0].start.second,
               command.periods[0].end.hour, command.periods[0].end.minute,
               command.periods[0].end.second, command.periods[0].value);
    rt_kprintf("%s test period[2] %04d-%02d-%02d %02d:%02d:%02d - %02d:%02d:%02d, power[%d]\n",
               get_char_time(),
               command.periods[1].start.year, command.periods[1].start.month,
               command.periods[1].start.day, command.periods[1].start.hour,
               command.periods[1].start.minute, command.periods[1].start.second,
               command.periods[1].end.hour, command.periods[1].end.minute,
               command.periods[1].end.second, command.periods[1].value);

    return Time_Ctrl_Set(&command);
}

/* 执行time_ctrl直接开始默认测试，执行time_ctrl stop停止并恢复额定输出。 */
static int time_ctrl(int argc, char **argv)
{
    Time_Ctrl_Result_t result;

    if((argc == 2) && (rt_strcmp(argv[1], "stop") == 0)) {
        result = Time_Ctrl_Stop();
        rt_kprintf("%s time_ctrl stop: %s\n", get_char_time(), Time_Ctrl_Result_Text(result));
        return (result == TIME_CTRL_RESULT_OK) ? 0 : -1;
    }

    if(argc != 1) {
        rt_kprintf("usage: time_ctrl\n");
        rt_kprintf("       time_ctrl stop\n");
        return -1;
    }

    result = time_ctrl_test_set_default();
    rt_kprintf("%s time_ctrl default test: %s (%d)\n",
               get_char_time(), Time_Ctrl_Result_Text(result), result);
    return (result == TIME_CTRL_RESULT_OK) ? 0 : -1;
}
MSH_CMD_EXPORT(time_ctrl, run default active-power time-control test);
