/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_
#define APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_

#include <rtthread.h>

#include "inverter_archive.h"
#include "inverter_protocol_library.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 周期抄读模块说明
 * ----------------
 * 本模块只负责“何时读、读哪一台、读哪个数据点、失败后如何恢复”，不直接操作
 * UART，也不负责把原始寄存器转换成最终工程量。这样可以把轮询调度、端口驱动和
 * 数据计算三部分解耦：
 *
 *   档案库 + 协议库 -> cycle_loop -> send_fn -> UART/无线发送
 *                              ^
 *                              +--- cycle_loop_rx_frame() 接收完整响应帧
 *                              |
 *                              +--- data_fn 交付抄读成功的原始寄存器
 *
 * 典型调用顺序：
 *   1. 系统完成档案库、协议库和通信端口初始化；
 *   2. 调用 cycle_loop_init() 注册发送及数据处理回调；
 *   3. 创建 cycle_loop_thread_entry 线程；
 *   4. 调用 cycle_loop_start() 启动周期抄读；
 *   5. 通信层收到完整响应帧后调用 cycle_loop_rx_frame()。
 */

/* 周期抄读的默认参数，后续可改为从配置区加载。所有时间参数单位均为毫秒。 */
#define CYCLE_LOOP_DEFAULT_PERIOD_MS          5000U
#define CYCLE_LOOP_DEFAULT_RESPONSE_TIMEOUT_MS 500U
#define CYCLE_LOOP_DEFAULT_REQUEST_GAP_MS       20U
#define CYCLE_LOOP_DEFAULT_RETRY_COUNT           2U

/*
 * Inv_ProtoData_t 当前由连续的 Inv_RegBlk_t 成员组成：电压、电流各包含 A/B/C
 * 三相，有功、无功和功率因数各包含 A/B/C 三相及总量。使用 sizeof 自动计算数量，
 * 可以避免增加新的运行数据点后忘记同步修改固定常量。
 */
#define CYCLE_LOOP_DATA_ITEM_COUNT \
    (sizeof(Inv_ProtoData_t) / sizeof(Inv_RegBlk_t))

/*
 * 周期抄读状态机：任何时刻最多只存在一个在途 Modbus 请求，避免同一通信链路上
 * 多个响应无法对应请求的问题。
 */
typedef enum Cycle_Loop_State
{
    CYCLE_LOOP_STATE_STOPPED = 0,    /* 已停止，不选择数据点，也不发送请求。 */
    CYCLE_LOOP_STATE_IDLE,           /* 一轮结束后的周期等待状态。 */
    CYCLE_LOOP_STATE_SELECT_ITEM,    /* 从档案库和协议库中选择下一个有效数据点。 */
    CYCLE_LOOP_STATE_WAIT_RESPONSE,  /* 请求已发出，等待完整响应帧或响应超时。 */
    CYCLE_LOOP_STATE_REQUEST_GAP     /* 两次请求之间的静默间隔，避免连续占用总线。 */
} Cycle_Loop_State_t;

typedef struct Cycle_Loop_Config
{
    rt_uint32_t period_ms;           /* 完成一轮抄读后，到下一轮开始的等待时间。 */
    rt_uint32_t response_timeout_ms; /* 单次请求等待响应的最长时间。 */
    rt_uint32_t request_gap_ms;      /* 当前数据点结束后到下一次请求的间隔。 */
    rt_uint8_t retry_count;          /* 首次请求失败后允许的最大重发次数。 */
} Cycle_Loop_Config_t;

/* 运行统计为累计值，重新调用 cycle_loop_init() 时清零。 */
typedef struct Cycle_Loop_Statistics
{
    rt_uint32_t cycle_count;   /* 已完成的整轮扫描次数，包括没有有效档案的空轮。 */
    rt_uint32_t request_count; /* 已成功交给发送回调的 Modbus 请求数，包含重试。 */
    rt_uint32_t success_count; /* 通过地址、功能码、长度和 CRC 校验的响应数。 */
    rt_uint32_t timeout_count; /* 等待响应超时次数；每次重试超时都会累计。 */
    rt_uint32_t error_count;   /* 某数据点用尽重试仍失败并被跳过的次数。 */
} Cycle_Loop_Statistics_t;

/*
 * 发送接口由端口管理层实现。port 使用 Inv_Port_t，frame 是已经包含 CRC 的完整
 * Modbus RTU 请求帧。成功发送完整报文返回 RT_EOK，端口不可用或发送长度不足时
 * 返回对应错误码。
 * 该抽象用于避免周期抄读模块直接假设物理端口和 UART 编号的映射关系。
 */
typedef rt_err_t (*cycle_loop_send_fn)(Inv_Port_t port,
                                       const rt_uint8_t *frame,
                                       rt_uint16_t frame_len);

/*
 * 单个数据点抄读成功后的交付接口：
 *   archive_index 对应 g_inv_archive_lib.slots[] 下标；
 *   data_index 对应 Inv_ProtoData_t 中按声明顺序排列的数据点下标；
 *   reg_blk 描述寄存器地址、数据类型、字节序和小数位；
 *   registers 为主机字节序的 16 位原始寄存器数组。
 * 所有指针只在本次回调期间有效，回调返回前必须完成复制；回调内不要长期阻塞。
 */
typedef void (*cycle_loop_data_fn)(rt_uint8_t archive_index,
                                   rt_uint8_t data_index,
                                   const Inv_RegBlk_t *reg_blk,
                                   const rt_uint16_t *registers,
                                   rt_uint16_t register_count);

/*
 * 初始化上下文并注册回调。config 为 RT_NULL 时使用本文件定义的默认参数。
 * 本函数只初始化，不自动开始发送；初始化成功后还需调用 cycle_loop_start()。
 */
rt_err_t cycle_loop_init(const Cycle_Loop_Config_t *config,
                         cycle_loop_send_fn send_fn,
                         cycle_loop_data_fn data_fn);

/* 立即安排第一轮抄读；未注册 send_fn 时返回 -RT_ENOSYS。 */
rt_err_t cycle_loop_start(void);

/* 停止状态机并丢弃尚未处理的接收帧；已发出的物理报文无法撤回。 */
void cycle_loop_stop(void);

/*
 * 通信接收层在获得一帧完整报文后调用。本函数会复制报文，不持有调用方缓冲区。
 * 当前没有等待响应或已有一帧待处理时返回 -RT_EBUSY；无关端口的帧会在状态机中
 * 被忽略，继续等待当前请求的正确响应。
 */
rt_err_t cycle_loop_rx_frame(Inv_Port_t port,
                             const rt_uint8_t *frame,
                             rt_uint16_t frame_len);

/* 查询当前状态以及累计统计；statistics 为 RT_NULL 时不执行任何操作。 */
Cycle_Loop_State_t cycle_loop_get_state(void);
void cycle_loop_get_statistics(Cycle_Loop_Statistics_t *statistics);

/*
 * RT-Thread 线程入口，需由应用线程表创建。线程只推进非阻塞状态机，等待响应期间
 * 不会阻塞通信接收线程。
 */
void cycle_loop_thread_entry(void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_ */
