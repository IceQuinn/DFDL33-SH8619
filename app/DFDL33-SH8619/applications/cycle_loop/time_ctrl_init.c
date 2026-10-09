/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "time_ctrl_init.h"

#include "AB_check.h"
#include "drv_ex_flash.h"
#include "inverter_archive.h"
#include "sys.h"
#include "user_ex_flash_mgmt.h"

/* 单档案需要掉电保存的配置。运行时状态不写入Flash。 */
typedef struct Time_Ctrl_Store_Entry
{
    uint8_t enabled;          /* 1表示重启后仍允许执行该档案的时段计划。 */
    uint8_t restore_pending;  /* 1表示逆变器可能仍在限功率，启动后必须先恢复100%。 */
    uint8_t reserved[2];      /* 保留字段，同时保证command按4字节边界对齐。 */
    Time_Ctrl_Command_t command; /* 该档案两个一次性控制时段的完整命令。 */
} Time_Ctrl_Store_Entry_t;

/* A/B区保存的完整时段控制配置。 */
typedef struct Time_Ctrl_Store
{
    rcd_head head; /* AB_check和AB_save使用的版本、长度及CRC校验头。 */
    uint16_t reserved; /* 保留字段，同时保证entries中的命令自然对齐。 */
    Time_Ctrl_Store_Entry_t entries[INVERTER_ARCHIVE_MAX_COUNT]; /* 12个固定档案槽位的配置。 */
} Time_Ctrl_Store_t;

/* 编译期保证单份配置不会越过为时段控制分配的4KB Flash扇区。 */
typedef char Time_Ctrl_Store_Size_Check_t[
    (sizeof(Time_Ctrl_Store_t) <= EXTFLASH_PAGE_SIZE) ? 1 : -1];

static Time_Ctrl_Store_t g_time_ctrl_store; /* 当前装载到RAM中的完整持久化配置。 */
static struct rt_mutex g_time_ctrl_store_mutex; /* 保护配置内容和批量保存状态。 */
static uint8_t g_time_ctrl_store_batch_depth; /* 大于0时延迟擦写，最外层End统一保存。 */
static rt_bool_t g_time_ctrl_store_dirty; /* RT_TRUE表示RAM配置尚未完整保存到A/B区。 */

/* 建立12个档案均禁用的默认配置，并固定每条命令对应的档案下标。 */
static void time_ctrl_store_set_default(void)
{
    uint8_t archive_index;

    rt_memset(&g_time_ctrl_store, 0, sizeof(g_time_ctrl_store));
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        g_time_ctrl_store.entries[archive_index].command.archive_index = archive_index;
    }
}

/* 在持有配置互斥锁时把完整RAM配置保存到Flash A/B区。 */
static rt_err_t time_ctrl_store_save_locked(void)
{
    if((is_power_down() != 0) && (is_power_down_legal_work() == 0)) {
        rt_kprintf("time control cfg save skipped while power down\n");
        return -RT_EBUSY;
    }

    return (AB_save(flash_write,
                    TIME_CTRL_CFG_ADDR_A,
                    TIME_CTRL_CFG_ADDR_B,
                    &g_time_ctrl_store,
                    TIME_CTRL_CFG_VERSION,
                    sizeof(g_time_ctrl_store),
                    "FLASH TIME CTRL CFG") == 0) ? RT_EOK : -RT_ERROR;
}

/* 只检查已配置时段的日期，不因整个计划已停止而跳过日期清理。 */
rt_bool_t Time_Ctrl_Init_Is_Expired(const Time_Ctrl_Command_t *command,
                                    const struct tm *today)
{
    uint8_t period_index;

    if((command == RT_NULL) || (today == RT_NULL)) {
        return RT_FALSE;
    }
    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index) {
        const Time_Ctrl_Period_t *period = &command->periods[period_index];

        if(period->enabled != RT_TRUE) {
            continue;
        }
        if((period->start.year != (uint16_t)(today->tm_year + 1900)) ||
           (period->start.month != (uint8_t)(today->tm_mon + 1)) ||
           (period->start.day != (uint8_t)today->tm_mday) ||
           (period->end.year != (uint16_t)(today->tm_year + 1900)) ||
           (period->end.month != (uint8_t)(today->tm_mon + 1)) ||
           (period->end.day != (uint8_t)today->tm_mday)) {
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

/* 清理旧计划及恢复标志，避免前一天未完成的恢复在次日重新下发。 */
rt_err_t Time_Ctrl_Init_Clear_Expired(const struct tm *today)
{
    uint8_t archive_index;
    rt_err_t result = RT_EOK;

    if(today == RT_NULL) {
        return -RT_EINVAL;
    }
    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        Time_Ctrl_Store_Entry_t *entry = &g_time_ctrl_store.entries[archive_index];

        if(Time_Ctrl_Init_Is_Expired(&entry->command, today) == RT_TRUE) {
            rt_memset(&entry->command, 0, sizeof(entry->command));
            entry->command.archive_index = archive_index;
            entry->enabled = 0U;
            entry->restore_pending = 0U;
            g_time_ctrl_store_dirty = RT_TRUE;
        }
    }
    if((g_time_ctrl_store_dirty == RT_TRUE) && (g_time_ctrl_store_batch_depth == 0U)) {
        result = time_ctrl_store_save_locked();
        if(result == RT_EOK) {
            g_time_ctrl_store_dirty = RT_FALSE;
        }
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return result;
}

/* 校验并装载Flash A/B区，无有效副本或版本不匹配时重建默认配置。 */
void Time_Ctrl_Init(void)
{
    int32_t check_result;
    time_t now;
    struct tm today;

    /* 本函数在创建业务线程前只调用一次，建立后由645线程和时段线程共同使用。 */
    if(rt_mutex_init(&g_time_ctrl_store_mutex, "tm_cfg", RT_IPC_FLAG_PRIO) != RT_EOK) {
        rt_kprintf("time control cfg mutex init failed\n");
        return;
    }

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    /* AB_check负责选择有效副本，并在仅一份有效时自动修复另一份。 */
    check_result = AB_check(flash_read,
                            flash_write,
                            TIME_CTRL_CFG_ADDR_A,
                            TIME_CTRL_CFG_ADDR_B,
                            &g_time_ctrl_store,
                            sizeof(g_time_ctrl_store),
                            "FLASH TIME CTRL CFG");
    if((check_result != 0) || (g_time_ctrl_store.head.ver != TIME_CTRL_CFG_VERSION)) {
        time_ctrl_store_set_default();
        (void)time_ctrl_store_save_locked();
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    /* 邮箱建立前清理非当天的启用或停止计划，RTC不可用时暂不清理。 */
    now = time(RT_NULL);
    if((now != (time_t)-1) && (localtime_r(&now, &today) != RT_NULL)) {
        if(Time_Ctrl_Init_Clear_Expired(&today) != RT_EOK) {
            rt_kprintf("time control expired cfg clear failed\n");
        }
    }
}

/* 在线程安全条件下读取一个档案的持久化配置副本。 */
rt_err_t Time_Ctrl_Init_Get(uint8_t archive_index,
                            Time_Ctrl_Command_t *command,
                            rt_bool_t *enabled,
                            rt_bool_t *restore_pending)
{
    const Time_Ctrl_Store_Entry_t *entry;

    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (command == RT_NULL) || (enabled == RT_NULL) || (restore_pending == RT_NULL)) {
        return -RT_EINVAL;
    }

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    entry = &g_time_ctrl_store.entries[archive_index];
    *command = entry->command;
    *enabled = (entry->enabled == 1U) ? RT_TRUE : RT_FALSE;
    *restore_pending = (entry->restore_pending == 1U) ? RT_TRUE : RT_FALSE;
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return RT_EOK;
}

/* 更新一个档案的命令和启用状态，批量模式下只标脏而不立即擦写。 */
rt_err_t Time_Ctrl_Init_Save_Command(const Time_Ctrl_Command_t *command,
                                     rt_bool_t enabled)
{
    Time_Ctrl_Store_Entry_t *entry;
    rt_err_t result;

    if((command == RT_NULL) ||
       (command->archive_index >= INVERTER_ARCHIVE_MAX_COUNT)) {
        return -RT_EINVAL;
    }

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    entry = &g_time_ctrl_store.entries[command->archive_index];
    entry->command = *command;
    entry->enabled = (enabled == RT_TRUE) ? 1U : 0U;
    g_time_ctrl_store_dirty = RT_TRUE;
    if(g_time_ctrl_store_batch_depth == 0U) {
        result = time_ctrl_store_save_locked();
        if(result == RT_EOK) {
            g_time_ctrl_store_dirty = RT_FALSE;
        }
    }
    else {
        result = RT_EOK;
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return result;
}

/* 进入批量更新模式，允许嵌套调用并延迟Flash擦写。 */
rt_err_t Time_Ctrl_Init_Batch_Begin(void)
{
    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    if(g_time_ctrl_store_batch_depth == UINT8_MAX) {
        rt_mutex_release(&g_time_ctrl_store_mutex);
        return -RT_EFULL;
    }
    ++g_time_ctrl_store_batch_depth;
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return RT_EOK;
}

/* 退出一层批量更新模式，最外层结束时把全部变化合并保存一次。 */
rt_err_t Time_Ctrl_Init_Batch_End(void)
{
    rt_err_t result = RT_EOK;

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    if(g_time_ctrl_store_batch_depth == 0U) {
        rt_mutex_release(&g_time_ctrl_store_mutex);
        return -RT_EINVAL;
    }
    --g_time_ctrl_store_batch_depth;
    if((g_time_ctrl_store_batch_depth == 0U) && (g_time_ctrl_store_dirty == RT_TRUE)) {
        result = time_ctrl_store_save_locked();
        if(result == RT_EOK) {
            g_time_ctrl_store_dirty = RT_FALSE;
        }
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return result;
}

/* 一次性关闭全部档案计划，不清除命令内容和安全恢复标志。 */
rt_err_t Time_Ctrl_Init_Disable_All(void)
{
    uint8_t archive_index;
    rt_err_t result;

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        g_time_ctrl_store.entries[archive_index].enabled = 0U;
    }
    g_time_ctrl_store_dirty = RT_TRUE;
    if(g_time_ctrl_store_batch_depth == 0U) {
        result = time_ctrl_store_save_locked();
        if(result == RT_EOK) {
            g_time_ctrl_store_dirty = RT_FALSE;
        }
    }
    else {
        result = RT_EOK;
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return result;
}

/* 更新安全恢复标志，相同值不重复保存，批量模式下与其他变化合并。 */
rt_err_t Time_Ctrl_Init_Save_Restore_Pending(uint8_t archive_index,
                                             rt_bool_t restore_pending)
{
    Time_Ctrl_Store_Entry_t *entry;
    uint8_t stored_value = (restore_pending == RT_TRUE) ? 1U : 0U;
    rt_err_t result = RT_EOK;

    if(archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
        return -RT_EINVAL;
    }

    rt_mutex_take(&g_time_ctrl_store_mutex, RT_WAITING_FOREVER);
    entry = &g_time_ctrl_store.entries[archive_index];
    if(entry->restore_pending != stored_value) {
        entry->restore_pending = stored_value;
        g_time_ctrl_store_dirty = RT_TRUE;
        if(g_time_ctrl_store_batch_depth == 0U) {
            result = time_ctrl_store_save_locked();
            if(result == RT_EOK) {
                g_time_ctrl_store_dirty = RT_FALSE;
            }
        }
    }
    rt_mutex_release(&g_time_ctrl_store_mutex);
    return result;
}

