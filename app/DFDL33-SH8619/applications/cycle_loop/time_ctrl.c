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
#include "modbus_master.h"
#include "user_rtc.h"

/* 时段控制线程的时间及状态轮询周期。 */
#define TIME_CTRL_POLL_MS                    200U

/* 恢复额定输出时使用的有功百分比。 */
#define TIME_CTRL_RESTORE_PERCENT            100

/* 必须与通用逆变器控制模块的内部数值缓冲区容量保持一致。 */
#define TIME_CTRL_CONTROL_REG_MAX               4U

/* 没有活动时段时使用的非法时段下标。 */
#define TIME_CTRL_NO_PERIOD                   -1

/* FinSH无参数测试命令使用的默认相对时间和控制值。 */
#define TIME_CTRL_TEST_PERIOD1_START_SEC       5
#define TIME_CTRL_TEST_PERIOD1_END_SEC        35
#define TIME_CTRL_TEST_PERIOD2_START_SEC      45
#define TIME_CTRL_TEST_PERIOD2_END_SEC        75
#define TIME_CTRL_TEST_PERCENT_VALUE          80
#define TIME_CTRL_TEST_POWER_VALUE          3000

/* 单个档案当前需要执行的异步控制动作。 */
typedef enum Time_Ctrl_Action
{
    TIME_CTRL_ACTION_NONE = 0, /* 当前没有待提交动作。 */
    TIME_CTRL_ACTION_START,    /* 提交时段规定的有功控制。 */
    TIME_CTRL_ACTION_RESTORE   /* 提交100%有功百分比恢复控制。 */
} Time_Ctrl_Action_t;

/* Set/Stop写入的单档案命令邮箱。 */
typedef struct Time_Ctrl_Mailbox
{
    Time_Ctrl_Command_t command; /* 该档案最新一次有效命令。 */
    uint32_t generation;         /* 每次Set或Stop递增的版本号。 */
    rt_bool_t enabled;           /* RT_TRUE表示允许执行邮箱命令。 */
} Time_Ctrl_Mailbox_t;

/* 轮询线程独占的单档案运行状态。 */
typedef struct Time_Ctrl_Context
{
    Time_Ctrl_Command_t command; /* 线程当前使用的命令副本。 */
    time_t starts[TIME_CTRL_PERIOD_COUNT]; /* 两个时段的开始时间戳。 */
    time_t ends[TIME_CTRL_PERIOD_COUNT];   /* 两个时段的结束时间戳。 */
    uint32_t generation;         /* 线程已经同步的邮箱版本号。 */
    uint8_t done_mask;           /* 已完成或已错过时段的位图。 */
    int8_t active_period;        /* 当前正在生效的时段下标，-1表示无。 */
    int8_t action_period;        /* 当前动作对应的时段下标，-1表示命令更新恢复。 */
    Time_Ctrl_Action_t action;   /* 当前待提交的控制动作。 */
    rt_bool_t enabled;           /* RT_TRUE表示当前命令允许启动新时段。 */
    rt_bool_t output_limited;    /* RT_TRUE表示已提交过限功率命令，需要恢复。 */
} Time_Ctrl_Context_t;

/* 12个档案相互独立的命令邮箱，由接口和轮询线程共享。 */
static Time_Ctrl_Mailbox_t g_time_ctrl_mailboxes[INVERTER_ARCHIVE_MAX_COUNT];

/* 12个档案相互独立的运行状态，只由轮询线程修改。 */
static Time_Ctrl_Context_t g_time_ctrl_contexts[INVERTER_ARCHIVE_MAX_COUNT];

/* RT_TRUE表示邮箱、运行状态和轮询线程均已初始化。 */
static volatile rt_bool_t g_time_ctrl_initialized;

/* 线程生成的控制流水号，所有档案共用且只在线程中递增。 */
static uint32_t g_time_ctrl_request_id;

/* 读取并检查一台逆变器指定有功控制方式的协议配置。 */
static rt_bool_t time_ctrl_get_control_config(uint8_t archive_index,
                                              Time_Ctrl_Mode_t mode,
                                              Inv_CtrlRegBlk_t *control);

/* 检查控制值能否由目标协议的数据类型和寄存器数量表示。 */
static rt_bool_t time_ctrl_value_valid(const Inv_CtrlRegBlk_t *control, int32_t value);

/* 按协议的小数位计算恢复额定输出所需的100%定点值。 */
static rt_bool_t time_ctrl_restore_value(uint8_t archive_index, int32_t *value);

/* 判断指定年份是否为公历闰年。 */
static rt_bool_t time_ctrl_is_leap_year(uint16_t year)
{
    return (((year % 4U) == 0U) &&
            (((year % 100U) != 0U) || ((year % 400U) == 0U))) ? RT_TRUE : RT_FALSE;
}

/* 返回指定年月的天数，月份无效时返回0。 */
static uint8_t time_ctrl_days_in_month(uint16_t year, uint8_t month)
{
    /* 平年每个月的固定天数。 */
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

/* 严格检查年月日时分秒，避免mktime自动归一化非法日期。 */
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

/* 判断两个日期时间是否属于同一个自然日。 */
static rt_bool_t time_ctrl_same_day(const Time_Ctrl_DateTime_t *left,
                                    const Time_Ctrl_DateTime_t *right)
{
    if((left == RT_NULL) || (right == RT_NULL)) {
        return RT_FALSE;
    }
    return ((left->year == right->year) && (left->month == right->month) &&
            (left->day == right->day)) ? RT_TRUE : RT_FALSE;
}

/* 将已经校验的本地日期时间转换为time_t。 */
static time_t time_ctrl_to_time(const Time_Ctrl_DateTime_t *value)
{
    struct tm local_time; /* 交给mktime的标准本地时间结构。 */

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

/* 校验一条命令，并返回两个时段的时间戳。 */
static Time_Ctrl_Result_t time_ctrl_validate(const Time_Ctrl_Command_t *command,
                                             time_t starts[TIME_CTRL_PERIOD_COUNT],
                                             time_t ends[TIME_CTRL_PERIOD_COUNT])
{
    uint8_t period_index; /* 当前正在校验的时段下标。 */

    if((command == RT_NULL) || (starts == RT_NULL) || (ends == RT_NULL)) {
        return TIME_CTRL_RESULT_INVALID_PARAMETER;
    }
    if(command->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        /* 当前时段的只读指针，简化后续字段访问。 */
        const Time_Ctrl_Period_t *period = &command->periods[period_index];

        if((period->mode != TIME_CTRL_ACTIVE_POWER_VALUE) &&
           (period->mode != TIME_CTRL_ACTIVE_POWER_PERCENT)) {
            return TIME_CTRL_RESULT_INVALID_PARAMETER;
        }
        // 非法时间检查
        if((time_ctrl_datetime_valid(&period->start) == RT_FALSE) ||
           (time_ctrl_datetime_valid(&period->end) == RT_FALSE)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        // 跨天检查
        if(time_ctrl_same_day(&period->start, &period->end) == RT_FALSE) {
            return TIME_CTRL_RESULT_CROSS_DAY;
        }

        starts[period_index] = time_ctrl_to_time(&period->start);
        ends[period_index] = time_ctrl_to_time(&period->end);
        if((starts[period_index] == (time_t)-1) ||
           (ends[period_index] == (time_t)-1)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        if(starts[period_index] >= ends[period_index]) {
            return TIME_CTRL_RESULT_INVALID_RANGE;
        }
    }

    /* 一条命令的两个时段也必须属于同一天，任何跨日期操作都直接拒绝。 */
    if(time_ctrl_same_day(&command->periods[0].start,
                          &command->periods[1].start) == RT_FALSE) {
        return TIME_CTRL_RESULT_CROSS_DAY;
    }

    /* 采用[start,end)区间，所以前一时段结束等于后一时段开始不算重叠。 */
    if((starts[0] < ends[1]) && (starts[1] < ends[0])) {
        return TIME_CTRL_RESULT_OVERLAP;
    }
    return TIME_CTRL_RESULT_OK;
}

/* 读取并检查一台逆变器指定有功控制方式的协议配置。 */
static rt_bool_t time_ctrl_get_control_config(uint8_t archive_index,
                                              Time_Ctrl_Mode_t mode,
                                              Inv_CtrlRegBlk_t *control)
{
    const Inv_Proto_t *protocol; /* 目标档案当前关联的协议对象。 */

    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) || (control == RT_NULL)) {
        return RT_FALSE;
    }
    protocol = Inv_Archive_Get_Protocol(archive_index);
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    if(mode == TIME_CTRL_ACTIVE_POWER_VALUE) {
        rt_memcpy(control, &protocol->ctrl.active_pwr_ctrl, sizeof(*control));
    }
    else if(mode == TIME_CTRL_ACTIVE_POWER_PERCENT) {
        rt_memcpy(control, &protocol->ctrl.active_pwr_pct_ctrl, sizeof(*control));
    }
    else {
        return RT_FALSE;
    }

    if((control->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) ||
       (control->reg_cnt == 0U) || (control->reg_cnt > TIME_CTRL_CONTROL_REG_MAX) ||
       ((control->write_func_code != MODBUS_FUNC_WRITE_SINGLE) &&
        (control->write_func_code != MODBUS_FUNC_WRITE_MULTIPLE))) {
        return RT_FALSE;
    }
    if((control->write_func_code == MODBUS_FUNC_WRITE_SINGLE) &&
       (control->reg_cnt != 1U)) {
        return RT_FALSE;
    }
    if((control->write_func_code == MODBUS_FUNC_WRITE_MULTIPLE) &&
       (control->reg_cnt > MODBUS_WRITE_REG_MAX)) {
        return RT_FALSE;
    }
    return RT_TRUE;
}

/* 检查控制值能否由目标协议的数据类型和寄存器数量表示。 */
static rt_bool_t time_ctrl_value_valid(const Inv_CtrlRegBlk_t *control, int32_t value)
{
    uint16_t byte_count; /* 协议控制寄存器能够携带的字节数量。 */

    if(control == RT_NULL) {
        return RT_FALSE;
    }
    byte_count = (uint16_t)control->reg_cnt * 2U;

    switch(control->data_type) {
    case TYPE_I8:
        return ((value >= INT8_MIN) && (value <= INT8_MAX)) ? RT_TRUE : RT_FALSE;

    case TYPE_U8:
        return ((value >= 0) && (value <= UINT8_MAX)) ? RT_TRUE : RT_FALSE;

    case TYPE_I16:
        return ((value >= INT16_MIN) && (value <= INT16_MAX)) ? RT_TRUE : RT_FALSE;

    case TYPE_U16:
        return ((value >= 0) && (value <= UINT16_MAX)) ? RT_TRUE : RT_FALSE;

    case TYPE_I32:
        return (byte_count >= 4U) ? RT_TRUE : RT_FALSE;

    case TYPE_U32:
    case TYPE_BIT_FIELD:
    case TYPE_BCD_TIME:
        return ((value >= 0) && (byte_count >= 4U)) ? RT_TRUE : RT_FALSE;

    case TYPE_FLOAT32:
        return (byte_count >= 4U) ? RT_TRUE : RT_FALSE;

    case TYPE_FLOAT64:
        return (byte_count >= 8U) ? RT_TRUE : RT_FALSE;

    case TYPE_ASCII:
    case TYPE_BCD:
    default:
        return RT_FALSE;
    }
}

/* 检查有功百分比定点值是否位于0%～100%闭区间。 */
static rt_bool_t time_ctrl_percent_value_valid(const Inv_CtrlRegBlk_t *control,
                                               int32_t value)
{
    int32_t maximum = TIME_CTRL_RESTORE_PERCENT; /* 当前小数位下100%的定点值。 */
    uint8_t decimal_index;                       /* 当前正在计算的小数位下标。 */

    if(control == RT_NULL) {
        return RT_FALSE;
    }
    for(decimal_index = 0U; decimal_index < control->decimal_places; ++decimal_index) {
        if(maximum > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        maximum *= 10;
    }
    return ((value >= 0) && (value <= maximum)) ? RT_TRUE : RT_FALSE;
}

/* 将时段控制结果码转换成固定英文说明。 */
const char *Time_Ctrl_Result_Text(Time_Ctrl_Result_t result)
{
    /* 枚举值和文本一一对应的固定查找表。 */
    static const char *texts[] = {
        "ok", "invalid parameter", "invalid time", "cross-day period",
        "start must be before end", "periods overlap", "invalid archive",
        "control unsupported", "invalid control value", "thread not initialized"
    };

    if((uint32_t)result >= (sizeof(texts) / sizeof(texts[0]))) {
        return "unknown error";
    }
    return texts[result];
}

/* 按值受理一台逆变器的命令；邮箱只在短临界区内更新。 */
Time_Ctrl_Result_t Time_Ctrl_Set(Time_Ctrl_Command_t command)
{
    time_t starts[TIME_CTRL_PERIOD_COUNT]; /* 仅用于本次参数校验的开始时间戳。 */
    time_t ends[TIME_CTRL_PERIOD_COUNT];   /* 仅用于本次参数校验的结束时间戳。 */
    Time_Ctrl_Result_t result;             /* 参数校验结果。 */
    Time_Ctrl_Mailbox_t *mailbox;          /* 目标档案的命令邮箱。 */
    rt_base_t level;                       /* 关中断前的CPU中断状态。 */
    Inv_CtrlRegBlk_t control;              /* 用于检查协议控制能力的临时配置。 */
    uint8_t period_index;                  /* 当前正在检查的时段下标。 */
    int32_t restore_value;                 /* 目标协议恢复100%时使用的定点值。 */

    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }

    result = time_ctrl_validate(&command, starts, ends);
    if(result != TIME_CTRL_RESULT_OK) {
        return result;
    }
    if(g_inv_archive_lib.valid[command.archive_index] != INVERTER_ARCHIVE_VALID) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    /* 两个调控方式以及结束时必需的百分比恢复方式都必须由目标协议支持。 */
    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        if(time_ctrl_get_control_config(command.archive_index,
                                        command.periods[period_index].mode,
                                        &control) == RT_FALSE) {
            return TIME_CTRL_RESULT_UNSUPPORTED;
        }
        if((time_ctrl_value_valid(&control,
                                  command.periods[period_index].value) == RT_FALSE) ||
           ((command.periods[period_index].mode == TIME_CTRL_ACTIVE_POWER_PERCENT) &&
            (time_ctrl_percent_value_valid(&control,
                                           command.periods[period_index].value) == RT_FALSE))) {
            return TIME_CTRL_RESULT_INVALID_VALUE;
        }
    }
    if(time_ctrl_get_control_config(command.archive_index,
                                    TIME_CTRL_ACTIVE_POWER_PERCENT,
                                    &control) == RT_FALSE) {
        return TIME_CTRL_RESULT_UNSUPPORTED;
    }
    if(time_ctrl_restore_value(command.archive_index, &restore_value) == RT_FALSE) {
        return TIME_CTRL_RESULT_INVALID_VALUE;
    }

    mailbox = &g_time_ctrl_mailboxes[command.archive_index];
    level = rt_hw_interrupt_disable();
    mailbox->command = command;
    mailbox->enabled = RT_TRUE;
    ++mailbox->generation;
    rt_hw_interrupt_enable(level);
    return TIME_CTRL_RESULT_OK;
}

/* 停止指定档案；线程看到新版本后决定是否需要先恢复额定功率。 */
Time_Ctrl_Result_t Time_Ctrl_Stop_Archive(uint8_t archive_index)
{
    Time_Ctrl_Mailbox_t *mailbox; /* 目标档案的命令邮箱。 */
    rt_base_t level;              /* 关中断前的CPU中断状态。 */

    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }
    if(archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    mailbox = &g_time_ctrl_mailboxes[archive_index];
    level = rt_hw_interrupt_disable();
    mailbox->enabled = RT_FALSE;
    ++mailbox->generation;
    rt_hw_interrupt_enable(level);
    return TIME_CTRL_RESULT_OK;
}

/* 停止全部档案；使用一次临界区保证线程看到一致的停止状态。 */
Time_Ctrl_Result_t Time_Ctrl_Stop(void)
{
    uint8_t archive_index; /* 当前正在停止的档案下标。 */
    rt_base_t level;       /* 关中断前的CPU中断状态。 */

    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }

    level = rt_hw_interrupt_disable();
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        g_time_ctrl_mailboxes[archive_index].enabled = RT_FALSE;
        ++g_time_ctrl_mailboxes[archive_index].generation;
    }
    rt_hw_interrupt_enable(level);
    return TIME_CTRL_RESULT_OK;
}

/* 按协议的小数位计算恢复额定输出所需的100%定点值。 */
static rt_bool_t time_ctrl_restore_value(uint8_t archive_index, int32_t *value)
{
    Inv_CtrlRegBlk_t ctrl;       /* 对齐后的有功百分比控制配置。 */
    int32_t scale = 1;           /* decimal_places对应的10次幂。 */
    uint8_t decimal_index;       /* 当前正在计算的小数位下标。 */

    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) || (value == RT_NULL)) {
        return RT_FALSE;
    }
    if(time_ctrl_get_control_config(archive_index,
                                    TIME_CTRL_ACTIVE_POWER_PERCENT,
                                    &ctrl) == RT_FALSE) {
        return RT_FALSE;
    }

    for(decimal_index = 0U; decimal_index < ctrl.decimal_places; ++decimal_index) {
        if(scale > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scale *= 10;
    }
    if(scale > (INT32_MAX / TIME_CTRL_RESTORE_PERCENT)) {
        return RT_FALSE;
    }

    *value = TIME_CTRL_RESTORE_PERCENT * scale;
    return ((time_ctrl_value_valid(&ctrl, *value) == RT_TRUE) &&
            (time_ctrl_percent_value_valid(&ctrl, *value) == RT_TRUE)) ?
           RT_TRUE : RT_FALSE;
}

/* 为一个档案生成控制请求并尝试放入通用异步控制队列。 */
static rt_err_t time_ctrl_submit(uint8_t archive_index,
                                 Time_Ctrl_Action_t action,
                                 const Time_Ctrl_Period_t *period)
{
    Inv_Control_Request_t request; /* 提交给通用控制模块的请求。 */

    if(archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return -RT_EINVAL;
    }

    rt_memset(&request, 0, sizeof(request));
    request.request_id = ++g_time_ctrl_request_id;
    request.archive_index = archive_index;

    if(action == TIME_CTRL_ACTION_RESTORE) {
        request.type = INV_CONTROL_ACTIVE_POWER_PERCENT;
        if(time_ctrl_restore_value(archive_index, &request.value) == RT_FALSE) {
            return -RT_EINVAL;
        }
    }
    else if((action == TIME_CTRL_ACTION_START) && (period != RT_NULL)) {
        request.type = (period->mode == TIME_CTRL_ACTIVE_POWER_PERCENT) ?
                       INV_CONTROL_ACTIVE_POWER_PERCENT : INV_CONTROL_ACTIVE_POWER;
        request.value = period->value;
    }
    else {
        return -RT_EINVAL;
    }

    return Inv_Control_Submit(&request);
}

/* 将一个档案的最新邮箱内容同步到线程运行状态。 */
static void time_ctrl_sync_mailbox(uint8_t archive_index, Time_Ctrl_Context_t *context)
{
    Time_Ctrl_Command_t command; /* 从共享邮箱复制出的最新命令。 */
    uint32_t generation;         /* 从共享邮箱读取的最新版本号。 */
    rt_bool_t enabled;           /* 从共享邮箱读取的最新启用状态。 */
    rt_base_t level;             /* 关中断前的CPU中断状态。 */

    level = rt_hw_interrupt_disable();
    generation = g_time_ctrl_mailboxes[archive_index].generation;
    enabled = g_time_ctrl_mailboxes[archive_index].enabled;
    command = g_time_ctrl_mailboxes[archive_index].command;
    rt_hw_interrupt_enable(level);

    if(generation == context->generation) {
        return;
    }

    context->command = command;
    context->enabled = enabled;
    context->generation = generation;
    context->done_mask = 0U;
    context->active_period = TIME_CTRL_NO_PERIOD;

    /* 已经提交过限功率控制时，新命令和Stop都必须先排队恢复额定输出。 */
    if(context->output_limited == RT_TRUE) {
        context->action = TIME_CTRL_ACTION_RESTORE;
        context->action_period = TIME_CTRL_NO_PERIOD;
    }
    /* 旧命令尚未开始时直接丢弃旧待执行动作并装载最新命令。 */
    else {
        context->action = TIME_CTRL_ACTION_NONE;
        context->action_period = TIME_CTRL_NO_PERIOD;
    }

    if(enabled == RT_TRUE) {
        /* Set已经校验过命令，此处只生成线程后续比较使用的时间戳。 */
        (void)time_ctrl_validate(&context->command, context->starts, context->ends);
    }
}

/* 完成一个已经成功进入通用控制队列的动作。 */
static void time_ctrl_finish_action(uint8_t archive_index, Time_Ctrl_Context_t *context)
{
    if(context->action == TIME_CTRL_ACTION_START) {
        context->active_period = context->action_period;
        context->output_limited = RT_TRUE;
        rt_kprintf("%s time control archive[%d] period[%d] request queued\n",
                   get_char_time(), archive_index + 1U, context->active_period + 1);
    }
    else {
        context->output_limited = RT_FALSE;
        if(context->action_period >= 0) {
            context->done_mask |= (uint8_t)(1U << (uint8_t)context->action_period);
        }
        rt_kprintf("%s time control archive[%d] restore request queued\n",
                   get_char_time(), archive_index + 1U);
    }

    context->action = TIME_CTRL_ACTION_NONE;
    context->action_period = TIME_CTRL_NO_PERIOD;
}

/* 尝试提交一个档案当前的START或RESTORE动作。 */
static void time_ctrl_process_action(uint8_t archive_index,
                                     Time_Ctrl_Context_t *context,
                                     time_t now)
{
    const Time_Ctrl_Period_t *period = RT_NULL; /* START动作对应的时段。 */
    rt_err_t submit_result;                    /* 通用控制队列的提交结果。 */

    if(context->action == TIME_CTRL_ACTION_NONE) {
        return;
    }

    /* START尚未入队就已到结束时间时取消启动，不再下发过期控制。 */
    if((context->action == TIME_CTRL_ACTION_START) &&
       (context->action_period >= 0) &&
       (now >= context->ends[(uint8_t)context->action_period])) {
        context->done_mask |= (uint8_t)(1U << (uint8_t)context->action_period);
        context->action = TIME_CTRL_ACTION_NONE;
        context->action_period = TIME_CTRL_NO_PERIOD;
        return;
    }

    if(context->action == TIME_CTRL_ACTION_START) {
        period = &context->command.periods[(uint8_t)context->action_period];
    }
    submit_result = time_ctrl_submit(archive_index, context->action, period);

    if(submit_result == RT_EOK) {
        time_ctrl_finish_action(archive_index, context);
        return;
    }

    /* 队列满或周期控制模块尚未初始化时保留动作，下一轮继续尝试。 */
    if((submit_result == -RT_EFULL) || (submit_result == -RT_EBUSY)) {
        return;
    }

    /* 档案失效或协议无法恢复时停止该档案，避免无限重试和执行不安全的新控制。 */
    rt_kprintf("%s time control archive[%d] submit failed[%d], schedule disabled\n",
               get_char_time(), archive_index + 1U, submit_result);
    context->enabled = RT_FALSE;
    /* RESTORE失败时保留受控标志，后续新命令仍必须先重新尝试恢复。 */
    if(context->action != TIME_CTRL_ACTION_RESTORE) {
        context->output_limited = RT_FALSE;
    }
    context->action = TIME_CTRL_ACTION_NONE;
    context->action_period = TIME_CTRL_NO_PERIOD;
    context->active_period = TIME_CTRL_NO_PERIOD;
}

/* 在空闲状态下查找当前应当启动的时段。 */
static void time_ctrl_find_period(Time_Ctrl_Context_t *context, time_t now)
{
    uint8_t period_index; /* 当前正在检查的时段下标。 */

    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        /* 当前时段在done_mask中对应的位。 */
        uint8_t period_bit = (uint8_t)(1U << period_index);

        if((context->done_mask & period_bit) != 0U) {
            continue;
        }
        if(now >= context->ends[period_index]) {
            /* 已经错过的时段只标记完成，不再补发历史控制。 */
            context->done_mask |= period_bit;
            continue;
        }
        if((now >= context->starts[period_index]) &&
           (now < context->ends[period_index])) {
            context->action = TIME_CTRL_ACTION_START;
            context->action_period = (int8_t)period_index;
            return;
        }
    }
}

/* 每轮推进一个档案的独立时段控制状态机。 */
static void time_ctrl_process_archive(uint8_t archive_index, time_t now)
{
    /* 当前档案的线程私有运行状态。 */
    Time_Ctrl_Context_t *context = &g_time_ctrl_contexts[archive_index];

    time_ctrl_sync_mailbox(archive_index, context);

    /* 档案失效后无法可靠恢复原设备，清除旧状态且不向新设备发送旧命令。 */
    if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) {
        context->enabled = RT_FALSE;
        context->output_limited = RT_FALSE;
        context->action = TIME_CTRL_ACTION_NONE;
        context->action_period = TIME_CTRL_NO_PERIOD;
        context->active_period = TIME_CTRL_NO_PERIOD;
        return;
    }

    /* 先处理上轮已经确定的动作，保证恢复请求总在新控制请求之前入队。 */
    if(context->action != TIME_CTRL_ACTION_NONE) {
        time_ctrl_process_action(archive_index, context, now);
        return;
    }

    /* Stop后的档案只允许完成恢复，不再启动新时段。 */
    if(context->enabled != RT_TRUE) {
        return;
    }

    /* 活动时段到达结束时间后，下一轮提交恢复额定功率请求。 */
    if((context->output_limited == RT_TRUE) &&
       (context->active_period >= 0) &&
       (now >= context->ends[(uint8_t)context->active_period])) {
        context->action = TIME_CTRL_ACTION_RESTORE;
        context->action_period = context->active_period;
        context->active_period = TIME_CTRL_NO_PERIOD;
        return;
    }

    /* 没有活动限功率时才允许寻找并启动下一时段。 */
    if(context->output_limited == RT_FALSE) {
        time_ctrl_find_period(context, now);
    }
}

/* 线程入口：初始化12个上下文，然后每200ms推进各档案的独立状态机。 */
void time_ctrl_thread_entry(void *parameter)
{
    uint8_t archive_index; /* 当前正在推进的档案下标。 */

    RT_UNUSED(parameter);

    rt_memset(g_time_ctrl_mailboxes, 0, sizeof(g_time_ctrl_mailboxes));
    rt_memset(g_time_ctrl_contexts, 0, sizeof(g_time_ctrl_contexts));
    for(archive_index = 0U;
        archive_index < INVERTER_ARCHIVE_MAX_COUNT;
        ++archive_index) {
        g_time_ctrl_contexts[archive_index].active_period = TIME_CTRL_NO_PERIOD;
        g_time_ctrl_contexts[archive_index].action_period = TIME_CTRL_NO_PERIOD;
    }
    g_time_ctrl_initialized = RT_TRUE;

    while(1) {
        /* 本轮所有档案共用同一个RTC时间，避免循环中产生时间边界差异。 */
        time_t now = time(RT_NULL);

        if(now != (time_t)-1) {
            for(archive_index = 0U;
                archive_index < INVERTER_ARCHIVE_MAX_COUNT;
                ++archive_index) {
                time_ctrl_process_archive(archive_index, now);
            }
        }

        rt_thread_mdelay(TIME_CTRL_POLL_MS);
    }
}

/* 将time_t转换为测试命令需要的年月日时分秒。 */
static rt_bool_t time_ctrl_test_datetime(time_t value, Time_Ctrl_DateTime_t *datetime)
{
    struct tm *local_time; /* localtime返回的本地时间结构。 */

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

/* 使用当前RTC时间生成测试命令中的两个默认时段。 */
static Time_Ctrl_Result_t time_ctrl_test_build_command(Time_Ctrl_Command_t *command)
{
    time_t now = time(RT_NULL); /* 生成测试时段时读取的当前RTC时间。 */

    if((command == RT_NULL) || (now == (time_t)-1)) {
        return TIME_CTRL_RESULT_INVALID_TIME;
    }

    rt_memset(command, 0, sizeof(*command));
    if((time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD1_START_SEC,
                                &command->periods[0].start) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD1_END_SEC,
                                &command->periods[0].end) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD2_START_SEC,
                                &command->periods[1].start) == RT_FALSE) ||
       (time_ctrl_test_datetime(now + TIME_CTRL_TEST_PERIOD2_END_SEC,
                                &command->periods[1].end) == RT_FALSE)) {
        return TIME_CTRL_RESULT_INVALID_TIME;
    }

    command->periods[0].mode = TIME_CTRL_ACTIVE_POWER_PERCENT;
    command->periods[0].value = TIME_CTRL_TEST_PERCENT_VALUE;
    command->periods[1].mode = TIME_CTRL_ACTIVE_POWER_VALUE;
    command->periods[1].value = TIME_CTRL_TEST_POWER_VALUE;
    return TIME_CTRL_RESULT_OK;
}

/* 按目标协议的小数位把测试用整数转换成控制定点值。 */
static rt_bool_t time_ctrl_test_scale_value(uint8_t archive_index,
                                            Time_Ctrl_Mode_t mode,
                                            int32_t integer_value,
                                            int32_t *scaled_value)
{
    Inv_CtrlRegBlk_t control; /* 目标控制方式的协议寄存器配置。 */
    int32_t scale = 1;        /* decimal_places对应的10次幂。 */
    uint8_t decimal_index;    /* 当前正在计算的小数位下标。 */

    if((scaled_value == RT_NULL) || (integer_value < 0) ||
       (time_ctrl_get_control_config(archive_index, mode, &control) == RT_FALSE)) {
        return RT_FALSE;
    }

    for(decimal_index = 0U; decimal_index < control.decimal_places; ++decimal_index) {
        if(scale > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scale *= 10;
    }
    if((integer_value != 0) && (scale > (INT32_MAX / integer_value))) {
        return RT_FALSE;
    }

    *scaled_value = integer_value * scale;
    return RT_TRUE;
}

/* 为当前全部有效档案设置同一组默认测试时段。 */
static Time_Ctrl_Result_t time_ctrl_test_set_default(void)
{
    Time_Ctrl_Command_t command;   /* 逐档案按值提交的测试命令。 */
    Time_Ctrl_Result_t result;     /* 构造或提交测试命令的结果。 */
    uint8_t archive_index;         /* 当前正在设置的档案下标。 */
    uint8_t accepted_count = 0U;   /* 已成功受理测试命令的档案数量。 */

    result = time_ctrl_test_build_command(&command);
    if(result != TIME_CTRL_RESULT_OK) {
        return result;
    }

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

    for(archive_index = 0U;
        archive_index < INVERTER_ARCHIVE_MAX_COUNT;
        ++archive_index) {
        if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) {
            continue;
        }

        command.archive_index = archive_index;
        if((time_ctrl_test_scale_value(archive_index,
                                       TIME_CTRL_ACTIVE_POWER_PERCENT,
                                       TIME_CTRL_TEST_PERCENT_VALUE,
                                       &command.periods[0].value) == RT_FALSE) ||
           (time_ctrl_test_scale_value(archive_index,
                                       TIME_CTRL_ACTIVE_POWER_VALUE,
                                       TIME_CTRL_TEST_POWER_VALUE,
                                       &command.periods[1].value) == RT_FALSE)) {
            return TIME_CTRL_RESULT_UNSUPPORTED;
        }
        result = Time_Ctrl_Set(command);
        if(result != TIME_CTRL_RESULT_OK) {
            return result;
        }
        ++accepted_count;
    }

    return (accepted_count > 0U) ? TIME_CTRL_RESULT_OK : TIME_CTRL_RESULT_ARCHIVE_INVALID;
}

/* FinSH入口：无参数运行默认测试，stop参数停止全部档案。 */
static int time_ctrl(int argc, char **argv)
{
    Time_Ctrl_Result_t result; /* 本次测试或停止操作的结果。 */

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
