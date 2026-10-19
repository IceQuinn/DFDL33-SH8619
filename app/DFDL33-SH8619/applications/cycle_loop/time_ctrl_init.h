/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __TIME_CTRL_INIT_H__
#define __TIME_CTRL_INIT_H__

#include <stdint.h>
#include <time.h>
#include <rtthread.h>

#include "time_ctrl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 外部Flash中时段控制配置的当前结构版本，保存结构变化时必须递增。 */
#define TIME_CTRL_CFG_VERSION 2U

/* 从Flash A/B区装载持久化配置，无有效配置时建立并保存默认空配置。 */
void Time_Ctrl_Init(void);

/* 判断有时段内容的命令是否不属于当天，空计划不需要重复清理。 */
rt_bool_t Time_Ctrl_Init_Is_Expired(const Time_Ctrl_Command_t *command,
                                    const struct tm *today);

/* 清除非当天计划（包括已停止计划）及恢复标志，并合并保存。 */
rt_err_t Time_Ctrl_Init_Clear_Expired(const struct tm *today);

/* 读取一个档案已经装载到RAM中的命令、启用状态和安全恢复标志。 */
rt_err_t Time_Ctrl_Init_Get(uint8_t archive_index,
                            Time_Ctrl_Command_t *command,
                            rt_bool_t *enabled,
                            rt_bool_t *restore_pending);

/* 保存一个档案的时段命令和启用状态，并保留该档案原有的安全恢复标志。 */
rt_err_t Time_Ctrl_Init_Save_Command(const Time_Ctrl_Command_t *command,
                                     rt_bool_t enabled);

/* 档案变更时彻底清除单台计划和恢复标志，不执行100%恢复。 */
rt_err_t Time_Ctrl_Init_Clear_Archive(uint8_t archive_index);

/* 批量设置期间只更新RAM，End时把全部变化合并成一次A/B区保存。 */
rt_err_t Time_Ctrl_Init_Batch_Begin(void);
rt_err_t Time_Ctrl_Init_Batch_End(void);

/* 一次Flash写入关闭全部计划，保留各档案命令内容和恢复标志。 */
rt_err_t Time_Ctrl_Init_Disable_All(void);

/* 更新一个档案的安全恢复标志，值没有变化时不重复擦写Flash。 */
rt_err_t Time_Ctrl_Init_Save_Restore_Pending(uint8_t archive_index,
                                             rt_bool_t restore_pending);

#ifdef __cplusplus
}
#endif /* __TIME_CTRL_INIT_H__ */

#endif
