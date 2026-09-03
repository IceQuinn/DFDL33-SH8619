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
    TIME_CTRL_ACTION_RESTORE   /* 按原控制方式提交额定有功功率恢复控制。 */
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
    uint8_t output_mode;         /* 最近一次已提交限功率命令的控制方式，用于选择恢复寄存器。 */
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

/* 读取并检查一台逆变器指定有功控制方式的协议配置。 */
static rt_bool_t time_ctrl_get_control_config(uint8_t archive_index,
                                              Time_Ctrl_Mode_t mode,
                                              Inv_CtrlRegBlk_t *control);

/* 检查控制值能否由目标协议的数据类型和寄存器数量表示。 */
static rt_bool_t time_ctrl_value_valid(const Inv_CtrlRegBlk_t *control, int32_t value);

/* 按协议的小数位计算百分比恢复额定输出所需的100%定点值。 */
static rt_bool_t time_ctrl_restore_value(uint8_t archive_index, int32_t *value);

/* 优先按额定有功功率生成数值恢复请求，条件不满足时退回100%百分比恢复。 */
static rt_bool_t time_ctrl_prepare_restore_request(uint8_t archive_index,
                                                   Time_Ctrl_Mode_t mode,
                                                   Inv_Control_Request_t *request);

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

    /* 月份不在1～12范围时不能查询月份天数。 */
    if((month == 0U) || (month > 12U)) {
        return 0U;
    }
    /* 闰年的2月比固定表中的平年2月多一天。 */
    if((month == 2U) && (time_ctrl_is_leap_year(year) == RT_TRUE)) {
        return 29U;
    }
    return days[month - 1U];
}

/* 严格检查年月日时分秒，避免mktime自动归一化非法日期。 */
static rt_bool_t time_ctrl_datetime_valid(const Time_Ctrl_DateTime_t *value)
{
    /* 指针、日期和时间任一字段越界时整组日期时间无效。 */
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
    /* 任一输入为空时不能比较两个日期。 */
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

    /* 命令或任一时间戳输出数组为空时参数无效。 */
    if((command == RT_NULL) || (starts == RT_NULL) || (ends == RT_NULL)) {
        return TIME_CTRL_RESULT_INVALID_PARAMETER;
    }
    /* 档案下标必须落在固定的12个档案槽位范围内。 */
    if(command->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    /* 两个时段逐项执行模式、日期和时间范围校验。 */
    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        /* 当前时段的只读指针，简化后续字段访问。 */
        const Time_Ctrl_Period_t *period = &command->periods[period_index];

        /* 时段只接受有功数值控和有功百分比控两种方式。 */
        if((period->mode != TIME_CTRL_ACTIVE_POWER_VALUE) &&
           (period->mode != TIME_CTRL_ACTIVE_POWER_PERCENT)) {
            return TIME_CTRL_RESULT_INVALID_PARAMETER;
        }
        /* 开始和结束时间都必须是严格有效的本地日期时间。 */
        if((time_ctrl_datetime_valid(&period->start) == RT_FALSE) ||
           (time_ctrl_datetime_valid(&period->end) == RT_FALSE)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        /* 单个控制时段不允许跨越自然日。 */
        if(time_ctrl_same_day(&period->start, &period->end) == RT_FALSE) {
            return TIME_CTRL_RESULT_CROSS_DAY;
        }

        starts[period_index] = time_ctrl_to_time(&period->start); /* 保存后续状态机使用的开始时间戳。 */
        ends[period_index] = time_ctrl_to_time(&period->end);     /* 保存后续状态机使用的结束时间戳。 */
        /* mktime无法转换任一边界时当前时段无效。 */
        if((starts[period_index] == (time_t)-1) ||
           (ends[period_index] == (time_t)-1)) {
            return TIME_CTRL_RESULT_INVALID_TIME;
        }
        /* 采用左闭右开区间，开始时间必须严格早于结束时间。 */
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
    protocol = Inv_Archive_Get_Protocol(archive_index); /* 通过档案中缓存的协议指针取得控制配置。 */
    /* 档案没有关联有效协议时不能取得控制寄存器。 */
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    /* 数值控读取普通有功功率控制寄存器配置。 */
    if(mode == TIME_CTRL_ACTIVE_POWER_VALUE) {
        rt_memcpy(control, &protocol->ctrl.active_pwr_ctrl, sizeof(*control));
    }
    /* 百分比控读取有功功率百分比控制寄存器配置。 */
    else if(mode == TIME_CTRL_ACTIVE_POWER_PERCENT) {
        rt_memcpy(control, &protocol->ctrl.active_pwr_pct_ctrl, sizeof(*control));
    }
    /* 其他模式不属于当前时段控制支持范围。 */
    else {
        return RT_FALSE;
    }

    /* 地址、数量或功能码任一不合法时该协议不支持对应控制方式。 */
    if((control->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) ||
       (control->reg_cnt == 0U) || (control->reg_cnt > TIME_CTRL_CONTROL_REG_MAX) ||
       ((control->write_func_code != MODBUS_FUNC_WRITE_SINGLE) &&
        (control->write_func_code != MODBUS_FUNC_WRITE_MULTIPLE))) {
        return RT_FALSE;
    }
    /* 06功能码只能写入一个16位寄存器。 */
    if((control->write_func_code == MODBUS_FUNC_WRITE_SINGLE) &&
       (control->reg_cnt != 1U)) {
        return RT_FALSE;
    }
    /* 10功能码的寄存器数量不能超过Modbus协议上限。 */
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

    /* 根据协议数据类型和可用字节数检查int32_t输入能否在线上表示。 */
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
    /* 按协议小数位逐位计算100%对应的定点整数上限。 */
    for(decimal_index = 0U; decimal_index < control->decimal_places; ++decimal_index) {
        /* 每次乘10前检查int32_t上溢。 */
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

    /* 结果枚举越过文本表范围时返回统一未知错误说明。 */
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
    Inv_Control_Request_t restore_request; /* 用于检查每个时段结束后的恢复方式。 */
    uint8_t period_index;                  /* 当前正在检查的时段下标。 */

    /* 时段线程尚未初始化邮箱和上下文时不能受理命令。 */
    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }

    result = time_ctrl_validate(&command, starts, ends); /* 先完成不依赖协议库的公共参数校验。 */
    /* 公共参数校验失败时原样返回具体错误码。 */
    if(result != TIME_CTRL_RESULT_OK) {
        return result;
    }
    /* 目标档案必须仍然有效，避免向已删除设备保存时段命令。 */
    if(g_inv_archive_lib.valid[command.archive_index] != INVERTER_ARCHIVE_VALID) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    /* 两个调控方式以及各自结束时需要使用的恢复方式都必须可用。 */
    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        /* 当前时段使用的控制寄存器必须在目标协议中有效配置。 */
        if(time_ctrl_get_control_config(command.archive_index,
                                        command.periods[period_index].mode,
                                        &control) == RT_FALSE) {
            return TIME_CTRL_RESULT_UNSUPPORTED;
        }
        /* 控制值必须符合数据类型范围，百分比值还必须位于0%～100%。 */
        if((time_ctrl_value_valid(&control,
                                  command.periods[period_index].value) == RT_FALSE) ||
           ((command.periods[period_index].mode == TIME_CTRL_ACTIVE_POWER_PERCENT) &&
            (time_ctrl_percent_value_valid(&control,
                                           command.periods[period_index].value) == RT_FALSE))) {
            return TIME_CTRL_RESULT_INVALID_VALUE;
        }

        /* 数值控优先使用本地额定功率恢复，无法使用时必须具备100%百分比兜底能力。 */
        if(time_ctrl_prepare_restore_request(command.archive_index,
                                             command.periods[period_index].mode,
                                             &restore_request) == RT_FALSE) {
            return TIME_CTRL_RESULT_UNSUPPORTED;
        }
    }

    mailbox = &g_time_ctrl_mailboxes[command.archive_index]; /* 定位目标档案对应的共享命令邮箱。 */
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

    /* 时段线程尚未初始化时没有可以停止的运行上下文。 */
    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }
    /* 停止接口只接受0～11档案槽位下标。 */
    if(archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return TIME_CTRL_RESULT_ARCHIVE_INVALID;
    }

    mailbox = &g_time_ctrl_mailboxes[archive_index]; /* 定位需要停止的单档案共享邮箱。 */
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

    /* 时段线程未初始化时不修改尚未建立的邮箱状态。 */
    if(g_time_ctrl_initialized != RT_TRUE) {
        return TIME_CTRL_RESULT_THREAD_ERROR;
    }

    level = rt_hw_interrupt_disable();
    /* 在同一临界区中关闭全部12个邮箱并递增各自版本号。 */
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

    /* 档案下标越界或恢复值输出指针为空时不能计算。 */
    /* 档案下标或输出地址无效时不能读取协议配置并返回恢复值。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) || (value == RT_NULL)) {
        return RT_FALSE;
    }
    /* 百分比恢复依赖目标协议支持有功功率百分比控制。 */
    if(time_ctrl_get_control_config(archive_index,
                                    TIME_CTRL_ACTIVE_POWER_PERCENT,
                                    &ctrl) == RT_FALSE) {
        return RT_FALSE;
    }

    /* 根据百分比控制寄存器的小数位逐位计算10的幂。 */
    for(decimal_index = 0U; decimal_index < ctrl.decimal_places; ++decimal_index) {
        /* 每次放大前检查比例因子是否会超出int32_t。 */
        if(scale > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scale *= 10;
    }
    /* 比例因子乘以100前再次检查最终恢复值是否溢出。 */
    if(scale > (INT32_MAX / TIME_CTRL_RESTORE_PERCENT)) {
        return RT_FALSE;
    }

    *value = TIME_CTRL_RESTORE_PERCENT * scale;
    return ((time_ctrl_value_valid(&ctrl, *value) == RT_TRUE) &&
            (time_ctrl_percent_value_valid(&ctrl, *value) == RT_TRUE)) ?
           RT_TRUE : RT_FALSE;
}

/* 将本地额定有功功率调整到数值控制寄存器的小数位后作为恢复值。 */
static rt_bool_t time_ctrl_Pn_restore_value(uint8_t archive_index, int32_t *value)
{
    const Inv_Proto_t *protocol;           /* 目标档案当前关联的协议对象。 */
    const Inv_RegBlk_t *Pn_config;         /* 协议库中的额定有功功率Pn读取配置。 */
    const Inv_RealtimeValue_t *Pn_data;    /* 周期抄读保存的本地额定有功功率Pn。 */
    Inv_CtrlRegBlk_t control;              /* 有功功率数值控制寄存器配置。 */
    int32_t scaled_value;                  /* 已换算成控制寄存器小数位的额定功率。 */
    uint8_t decimal_places;                /* 换算过程中当前数值的小数位数。 */

    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) || (value == RT_NULL)) {
        return RT_FALSE;
    }

    protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得档案绑定的协议库，后续读取额定功率配置。 */
    /* 档案未绑定协议库时不能判断额定功率寄存器是否受支持。 */
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    Pn_config = &protocol->param.Pn; /* 取得额定有功功率Pn的协议寄存器配置。 */
    Pn_data = &g_inv_data[archive_index].param.Pn; /* 取得最近一次周期抄读的额定有功功率Pn。 */

    /* 额定功率读取寄存器未配置、功能码不支持或本地尚无有效值时改用百分比恢复。 */
    if((Pn_config->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) ||
       (Pn_config->reg_cnt == 0U) ||
       ((Pn_config->read_func_code != MODBUS_FUNC_READ_HOLDING) &&
        (Pn_config->read_func_code != MODBUS_FUNC_READ_INPUT)) ||
       (Pn_data->valid == 0U) || (Pn_data->value <= 0)) {
        return RT_FALSE;
    }

    /* 数值控制寄存器不可用时不能通过额定有功功率数值恢复。 */
    if(time_ctrl_get_control_config(archive_index,
                                    TIME_CTRL_ACTIVE_POWER_VALUE,
                                    &control) == RT_FALSE) {
        return RT_FALSE;
    }

    scaled_value = Pn_data->value;                 /* 从Pn读取寄存器使用的小数位开始换算。 */
    decimal_places = Pn_config->decimal_places;    /* 记录当前scaled_value对应的小数位。 */

    /* 控制寄存器小数位更多时放大本地定点值，并在每一步检查int32_t溢出。 */
    while(decimal_places < control.decimal_places) {
        /* 放大前检查正数额定功率是否会超过int32_t可表示范围。 */
        if(scaled_value > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scaled_value *= 10;
        ++decimal_places;
    }

    /* 控制寄存器小数位更少时缩小本地定点值，无法表示的小数部分按整数除法舍去。 */
    while(decimal_places > control.decimal_places) {
        scaled_value /= 10;
        --decimal_places;
    }

    /* 换算结果必须为正数并满足目标控制寄存器配置的上下限。 */
    if((scaled_value <= 0) || (time_ctrl_value_valid(&control, scaled_value) == RT_FALSE)) {
        return RT_FALSE;
    }

    *value = scaled_value; /* 将校验通过的额定功率定点值返回给恢复请求。 */
    return RT_TRUE;
}

/* 数值控优先写入本地额定有功功率，其余情况统一使用100%百分比恢复。 */
static rt_bool_t time_ctrl_prepare_restore_request(uint8_t archive_index,
                                                   Time_Ctrl_Mode_t mode,
                                                   Inv_Control_Request_t *request)
{
    /* 请求输出地址为空时无法填写恢复控制类型和控制值。 */
    if(request == RT_NULL) {
        return RT_FALSE;
    }

    /* 只有原时段使用数值控且额定功率配置、本地值和数值寄存器均有效时才按数值恢复。 */
    if((mode == TIME_CTRL_ACTIVE_POWER_VALUE) &&
       (time_ctrl_Pn_restore_value(archive_index, &request->value) == RT_TRUE)) {
        request->type = INV_CONTROL_ACTIVE_POWER;
        return RT_TRUE;
    }

    request->type = INV_CONTROL_ACTIVE_POWER_PERCENT; /* 数值恢复不可用时退回100%功率百分比恢复。 */
    return time_ctrl_restore_value(archive_index, &request->value);
}

/* 为一个档案生成控制请求并尝试放入通用异步控制队列。 */
static rt_err_t time_ctrl_submit(uint8_t archive_index,
                                 Time_Ctrl_Action_t action,
                                 const Time_Ctrl_Period_t *period,
                                 Time_Ctrl_Mode_t restore_mode)
{
    Inv_Control_Request_t request; /* 提交给通用控制模块的请求。 */

    /* 档案下标越界时禁止生成可能写向未知设备的控制请求。 */
    if(archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return -RT_EINVAL;
    }

    rt_memset(&request, 0, sizeof(request));             /* 清零请求中未显式赋值的保留字段。 */
    request.request_id = Inv_Control_Allocate_Request_Id(); /* 从公共分配器取得跨控制来源唯一的请求编号。 */
    request.archive_index = archive_index;              /* 指定本次控制对应的档案。 */

    /* 恢复动作根据原控制方式优先选择额定功率数值恢复或100%百分比恢复。 */
    if(action == TIME_CTRL_ACTION_RESTORE) {
        /* 恢复请求无法取得安全控制方式和值时拒绝入队。 */
        if(time_ctrl_prepare_restore_request(archive_index,
                                             restore_mode,
                                             &request) == RT_FALSE) {
            return -RT_EINVAL;
        }
    }
    /* 启动动作直接使用当前时段已经校验过的控制方式和值。 */
    else if((action == TIME_CTRL_ACTION_START) && (period != RT_NULL)) {
        request.type = (period->mode == TIME_CTRL_ACTIVE_POWER_PERCENT) ?
                       INV_CONTROL_ACTIVE_POWER_PERCENT : INV_CONTROL_ACTIVE_POWER;
        request.value = period->value;
    }
    /* 其余动作类型或缺少时段配置均属于内部状态异常。 */
    else {
        return -RT_EINVAL;
    }

    return Inv_Control_Submit(&request); /* 复用周期抄读模块的异步控制队列，不在时段线程内等待结果。 */
}

/* 将一个档案的最新邮箱内容同步到线程运行状态。 */
static void time_ctrl_sync_mailbox(uint8_t archive_index, Time_Ctrl_Context_t *context)
{
    Time_Ctrl_Command_t command; /* 从共享邮箱复制出的最新命令。 */
    uint32_t generation;         /* 从共享邮箱读取的最新版本号。 */
    rt_bool_t enabled;           /* 从共享邮箱读取的最新启用状态。 */
    rt_base_t level;             /* 关中断前的CPU中断状态。 */

    level = rt_hw_interrupt_disable(); /* 禁止Set或Stop在复制复合命令期间更新邮箱。 */
    generation = g_time_ctrl_mailboxes[archive_index].generation;
    enabled = g_time_ctrl_mailboxes[archive_index].enabled;
    command = g_time_ctrl_mailboxes[archive_index].command;
    rt_hw_interrupt_enable(level); /* 邮箱快照完整后立即恢复原中断状态。 */

    /* 版本号相同表示本轮没有新命令，不需要重置运行状态。 */
    if(generation == context->generation) {
        return;
    }

    context->command = command;       /* 用最新命令覆盖线程私有命令副本。 */
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

    /* 仅为启用命令生成时间戳，Stop命令不再需要解析时段。 */
    if(enabled == RT_TRUE) {
        /* Set已经校验过命令，此处只生成线程后续比较使用的时间戳。 */
        (void)time_ctrl_validate(&context->command, context->starts, context->ends);
    }
}

/* 完成一个已经成功进入通用控制队列的动作。 */
static void time_ctrl_finish_action(uint8_t archive_index, Time_Ctrl_Context_t *context)
{
    /* START成功入队后记录当前受控时段和恢复时需要使用的控制方式。 */
    if(context->action == TIME_CTRL_ACTION_START) {
        context->active_period = context->action_period;
        context->output_mode = (uint8_t)context->command.periods[(uint8_t)context->action_period].mode;
        context->output_limited = RT_TRUE;
        rt_kprintf("%s time control archive[%d] period[%d] request queued\n",
                   get_char_time(), archive_index + 1U, context->active_period + 1);
    }
    /* 非START动作只能是RESTORE，恢复入队后清除限功率状态并标记时段完成。 */
    else {
        context->output_limited = RT_FALSE;
        /* 正常时段结束触发恢复时记录done位，Stop触发的恢复没有对应时段。 */
        if(context->action_period >= 0) {
            context->done_mask |= (uint8_t)(1U << (uint8_t)context->action_period);
        }
        rt_kprintf("%s time control archive[%d] restore request queued\n",
                   get_char_time(), archive_index + 1U);
    }

    context->action = TIME_CTRL_ACTION_NONE; /* 当前动作已进入队列，状态机返回空闲动作状态。 */
    context->action_period = TIME_CTRL_NO_PERIOD;
}

/* 尝试提交一个档案当前的START或RESTORE动作。 */
static void time_ctrl_process_action(uint8_t archive_index,
                                     Time_Ctrl_Context_t *context,
                                     time_t now)
{
    const Time_Ctrl_Period_t *period = RT_NULL; /* START动作对应的时段。 */
    rt_err_t submit_result;                    /* 通用控制队列的提交结果。 */

    /* 没有待提交动作时直接返回，避免无意义访问时段数组。 */
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

    /* START需要向提交函数传入时段，RESTORE不依赖period参数。 */
    if(context->action == TIME_CTRL_ACTION_START) {
        period = &context->command.periods[(uint8_t)context->action_period];
    }
    submit_result = time_ctrl_submit(archive_index, /* 尝试将动作放入通用控制队列。 */
                                     context->action,
                                     period,
                                     (Time_Ctrl_Mode_t)context->output_mode);

    /* 成功入队后立即更新时段状态，控制执行结果由通用控制模块独立处理。 */
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

        /* 已完成或已错过的时段不允许重复启动。 */
        if((context->done_mask & period_bit) != 0U) {
            continue;
        }
        /* 当前时间已越过结束点时跳过该历史时段。 */
        if(now >= context->ends[period_index]) {
            /* 已经错过的时段只标记完成，不再补发历史控制。 */
            context->done_mask |= period_bit;
            continue;
        }
        /* 当前时间落在未完成时段内时生成START动作，由下一轮统一提交。 */
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

    time_ctrl_sync_mailbox(archive_index, context); /* 先接收Set或Stop留下的最新邮箱状态。 */

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
        time_ctrl_process_action(archive_index, context, now); /* 提交待处理动作并保留队列满时的重试状态。 */
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
        time_ctrl_find_period(context, now); /* 从两个配置时段中寻找当前应启动的时段。 */
    }
}

/* 线程入口：初始化12个上下文，然后每200ms推进各档案的独立状态机。 */
void time_ctrl_thread_entry(void *parameter)
{
    uint8_t archive_index; /* 当前正在推进的档案下标。 */

    RT_UNUSED(parameter); /* RT-Thread线程入口参数当前未使用。 */

    rt_memset(g_time_ctrl_mailboxes, 0, sizeof(g_time_ctrl_mailboxes)); /* 清除上电前不存在的外部命令。 */
    rt_memset(g_time_ctrl_contexts, 0, sizeof(g_time_ctrl_contexts));   /* 清除全部档案的线程运行状态。 */
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        g_time_ctrl_contexts[archive_index].active_period = TIME_CTRL_NO_PERIOD;
        g_time_ctrl_contexts[archive_index].action_period = TIME_CTRL_NO_PERIOD;
    }
    g_time_ctrl_initialized = RT_TRUE; /* 上下文初始化完成后才允许外部Set和Stop访问邮箱。 */

    while(1) {
        /* 本轮所有档案共用同一个RTC时间，避免循环中产生时间边界差异。 */
        time_t now = time(RT_NULL);

        /* RTC读取成功时才推进控制，避免无效时间误触发历史时段。 */
        if(now != (time_t)-1) {
            /* 逐档案推进独立状态，某一档案队列满不会阻塞其他档案。 */
            for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
                time_ctrl_process_archive(archive_index, now); /* 每个档案保持独立的时段控制状态。 */
            }
        }

        rt_thread_mdelay(TIME_CTRL_POLL_MS); /* 固定节拍轮询命令版本和时段边界。 */
    }
}

/* 将time_t转换为测试命令需要的年月日时分秒。 */
static rt_bool_t time_ctrl_test_datetime(time_t value, Time_Ctrl_DateTime_t *datetime)
{
    struct tm *local_time; /* localtime返回的本地时间结构。 */

    /* 输出结构为空时不能写入转换后的日期时间。 */
    if(datetime == RT_NULL) {
        return RT_FALSE;
    }
    local_time = localtime(&value); /* 使用系统时区把时间戳转换成本地日历时间。 */
    /* C库转换失败时拒绝生成不完整的测试时段。 */
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

    /* 命令地址为空或RTC无效时不能按当前时间构造测试计划。 */
    if((command == RT_NULL) || (now == (time_t)-1)) {
        return TIME_CTRL_RESULT_INVALID_TIME;
    }

    rt_memset(command, 0, sizeof(*command)); /* 清除命令中未填写的保留内容。 */
    /* 任一时间点转换失败都会使两个测试时段整体无效。 */
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

    /* 输出地址、整数值或协议配置不合法时禁止生成测试控制值。 */
    if((scaled_value == RT_NULL) || (integer_value < 0) ||
       (time_ctrl_get_control_config(archive_index, mode, &control) == RT_FALSE)) {
        return RT_FALSE;
    }

    /* 按控制寄存器小数位计算定点比例，并逐次检查比例因子溢出。 */
    for(decimal_index = 0U; decimal_index < control.decimal_places; ++decimal_index) {
        /* 乘10前检查本次比例因子是否仍可由int32_t表示。 */
        if(scale > (INT32_MAX / 10)) {
            return RT_FALSE;
        }
        scale *= 10;
    }
    /* 非零测试整数与比例因子相乘前检查最终值是否溢出。 */
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

    result = time_ctrl_test_build_command(&command); /* 先按当前RTC构造公共测试时段。 */
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

    /* 每个协议的小数位可能不同，因此逐档案换算并提交独立命令。 */
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        /* 仅对有效档案生成测试控制，避免访问未绑定协议的档案。 */
        if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) {
            continue;
        }

        command.archive_index = archive_index; /* 将公共时段绑定到当前档案。 */
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
        result = Time_Ctrl_Set(command); /* 将已经按当前协议缩放的命令写入对应邮箱。 */
        if(result != TIME_CTRL_RESULT_OK) {
            return result;
        }
        ++accepted_count;
    }

    return (accepted_count > 0U) ? TIME_CTRL_RESULT_OK : TIME_CTRL_RESULT_ARCHIVE_INVALID;
}

/* 将控制方式转换成打印使用的固定文本。 */
static const char *time_ctrl_mode_text(Time_Ctrl_Mode_t mode)
{
    /* 数值控制方式打印为active_power。 */
    if(mode == TIME_CTRL_ACTIVE_POWER_VALUE) {
        return "active_power";
    }
    /* 百分比控制方式打印为active_power_percent。 */
    if(mode == TIME_CTRL_ACTIVE_POWER_PERCENT) {
        return "active_power_percent";
    }
    return "unknown";
}

/* 打印全部有效档案中已经启用的时段控制，没有配置的档案不输出。 */
void Time_Ctrl_Print_All(void)
{
    uint8_t archive_index;       /* 当前正在检查的档案下标。 */
    uint8_t period_index;        /* 当前正在打印的时段下标。 */
    rt_bool_t enabled;           /* 当前档案邮箱中的时段控制启用状态。 */
    Time_Ctrl_Command_t command; /* 在临界区内取得的单档案命令副本。 */
    rt_base_t level;             /* 关中断前的CPU中断状态。 */

    /* 线程尚未初始化邮箱时没有可安全读取的时段配置。 */
    if(g_time_ctrl_initialized != RT_TRUE) {
        return;
    }

    /* 逐个检查有效档案，并从邮箱复制已启用的命令。 */
    for(archive_index = 0U;
        archive_index < INVERTER_ARCHIVE_MAX_COUNT;
        ++archive_index) {
        /* 无效档案不属于当前档案库，因此不打印其邮箱中的历史命令。 */
        if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) {
            continue;
        }

        /* 命令结构整体复制期间禁止Set或Stop改写同一个邮箱。 */
        level = rt_hw_interrupt_disable();
        enabled = g_time_ctrl_mailboxes[archive_index].enabled;
        /* 只有已启用邮箱才复制命令，关闭邮箱中的旧command不参与打印。 */
        if(enabled == RT_TRUE) {
            command = g_time_ctrl_mailboxes[archive_index].command;
        }
        rt_hw_interrupt_enable(level);

        /* 当前有效档案未启用时段控制时不输出空配置。 */
        if(enabled != RT_TRUE) {
            continue;
        }

        /* 每个启用档案固定打印两个时段。 */
        for(period_index = 0U;
            period_index < TIME_CTRL_PERIOD_COUNT;
            ++period_index) {
            /* 当前打印时段的只读指针，用于缩短字段访问表达式。 */
            const Time_Ctrl_Period_t *period = &command.periods[period_index];

            rt_kprintf("archive[%d] period[%d] "
                       "%04d-%02d-%02d %02d:%02d:%02d - "
                       "%04d-%02d-%02d %02d:%02d:%02d "
                       "mode[%s] value[%d]\n",
                       archive_index + 1U,
                       period_index + 1U,
                       period->start.year,
                       period->start.month,
                       period->start.day,
                       period->start.hour,
                       period->start.minute,
                       period->start.second,
                       period->end.year,
                       period->end.month,
                       period->end.day,
                       period->end.hour,
                       period->end.minute,
                       period->end.second,
                       time_ctrl_mode_text(period->mode),
                       period->value);
        }
    }
}

/* FinSH入口：打印全部有效档案中已经启用的时段控制。 */
static int time_ctrl_print(int argc, char **argv)
{
    RT_UNUSED(argv);

    /* 打印命令不接收参数，参数数量不符时只显示用法。 */
    if(argc != 1) {
        rt_kprintf("usage: time_ctrl_print\n");
        return -1;
    }

    Time_Ctrl_Print_All(); /* 输出所有有效且已启用档案的两个时段。 */
    return 0;
}
MSH_CMD_EXPORT(time_ctrl_print, print enabled time controls in all archives);


/* FinSH入口：无参数运行默认测试，stop参数停止全部档案。 */
static int time_ctrl(int argc, char **argv)
{
    Time_Ctrl_Result_t result; /* 本次测试或停止操作的结果。 */

    /* stop子命令关闭全部档案，并为仍受控的档案安排恢复动作。 */
    if((argc == 2) && (rt_strcmp(argv[1], "stop") == 0)) {
        result = Time_Ctrl_Stop();
        rt_kprintf("%s time_ctrl stop: %s\n", get_char_time(), Time_Ctrl_Result_Text(result));
        return (result == TIME_CTRL_RESULT_OK) ? 0 : -1;
    }

    /* 除无参数测试和stop外不接受其他命令形式。 */
    if(argc != 1) {
        rt_kprintf("usage: time_ctrl\n");
        rt_kprintf("       time_ctrl stop\n");
        return -1;
    }

    result = time_ctrl_test_set_default(); /* 无参数时为全部有效档案创建默认测试时段。 */
    rt_kprintf("%s time_ctrl default test: %s (%d)\n",
               get_char_time(), Time_Ctrl_Result_Text(result), result);
    return (result == TIME_CTRL_RESULT_OK) ? 0 : -1;
}
MSH_CMD_EXPORT(time_ctrl, run default active-power time-control test);
