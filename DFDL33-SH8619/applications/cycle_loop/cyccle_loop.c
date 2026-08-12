/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cycle_loop.h"

#include <rthw.h>

#include "modbus_master.h"
#include "user_comm.h"

#define CYCLE_LOOP_THREAD_POLL_MS             10U
#define CYCLE_LOOP_RX_FRAME_SIZE              MODBUS_RTU_ADU_MAX

/*
 * 周期抄读的唯一运行上下文。
 *
 * archive_index 和 data_index 共同组成当前扫描游标：前者定位档案槽位，后者定位
 * Inv_ProtoData_t 中的数据点。request_* 保存当前在途请求的快照，保证协议库内容
 * 即使在等待响应期间被其它模块刷新，响应仍按照发送时的参数进行校验。
 *
 * rx_* 是通信接收线程与周期抄读线程之间的单帧邮箱。写入和取出时使用临界区，
 * 防止复制过程中另一个执行上下文看到半帧数据。
 */
typedef struct Cycle_Loop_Context
{
    Cycle_Loop_Config_t config;           /* 生效中的时间及重试配置。 */
    Cycle_Loop_Statistics_t statistics;   /* 自最近一次初始化以来的累计统计。 */
    cycle_loop_send_fn send_fn;           /* 端口发送适配接口。 */
    cycle_loop_data_fn data_fn;           /* 抄读成功后的原始数据交付接口。 */

    volatile Cycle_Loop_State_t state;    /* 当前状态；接收接口会读取此字段。 */
    rt_uint8_t archive_index;             /* 当前档案槽位下标，范围 0~11。 */
    rt_uint8_t data_index;                /* 当前运行数据点下标。 */
    rt_uint8_t retry_count;               /* 当前数据点已经执行的重发次数。 */

    Inv_Port_t request_port;              /* 当前请求使用的物理接入端口。 */
    Inv_RegBlk_t request_reg;             /* 当前请求对应的协议寄存器描述。 */
    rt_uint16_t request_register_count;   /* 协议库中配置的寄存器个数。 */
    rt_tick_t state_tick;                 /* 进入等待/间隔状态时的节拍。 */
    rt_tick_t next_cycle_tick;            /* 下一轮允许开始的绝对节拍。 */

    rt_uint8_t rx_frame[CYCLE_LOOP_RX_FRAME_SIZE]; /* 待处理的完整 Modbus 帧。 */
    rt_uint16_t rx_frame_len;             /* rx_frame 中的有效字节数。 */
    Inv_Port_t rx_port;                   /* 接收到该帧的物理端口。 */
    volatile rt_bool_t rx_ready;          /* RT_TRUE 表示邮箱中存在待处理帧。 */
} Cycle_Loop_Context_t;

static Cycle_Loop_Context_t g_cycle_loop;

static rt_tick_t cycle_loop_ms_to_tick(rt_uint32_t milliseconds)
{
    rt_tick_t tick = rt_tick_from_millisecond(milliseconds);

    /* RT_TICK_PER_SECOND 较低时，小于一个 tick 的非零延时至少等待一个 tick。 */
    return (tick == 0U) ? 1U : tick;
}

/* 使用有符号差值判断绝对节拍到期，可兼容 rt_tick_t 自然回绕。 */
static rt_bool_t cycle_loop_tick_expired(rt_tick_t now, rt_tick_t deadline)
{
    return ((rt_int32_t)(now - deadline) >= 0) ? RT_TRUE : RT_FALSE;
}

static void cycle_loop_load_default_config(Cycle_Loop_Config_t *config)
{
    config->period_ms = CYCLE_LOOP_DEFAULT_PERIOD_MS;
    config->response_timeout_ms = CYCLE_LOOP_DEFAULT_RESPONSE_TIMEOUT_MS;
    config->request_gap_ms = CYCLE_LOOP_DEFAULT_REQUEST_GAP_MS;
    config->retry_count = CYCLE_LOOP_DEFAULT_RETRY_COUNT;
}

/*
 * 根据厂家名称和规约版本匹配档案使用的协议。
 * mfr_info 是固定长度的线上结构，名称不保证以 '\0' 结尾，因此必须整体比较，
 * 不能使用 strcmp()。
 */
static const Inv_Proto_t *cycle_loop_find_protocol(const Inv_Archive_t *archive)
{
    rt_uint16_t index;

    for(index = 0U; index < INVERTER_PROTOCOL_LIBRARY_COUNT; ++index)
    {
        const Inv_Proto_t *protocol = &g_inv_proto_lib.proto[index];

        if((protocol->valid == INVERTER_PROTOCOL_VALID) &&
           (rt_memcmp(&protocol->mfr_info,
                      &archive->mfr_info,
                      sizeof(archive->mfr_info)) == 0))
        {
            return protocol;
        }
    }

    return RT_NULL;
}

/* Inv_Proto_t 按 1 字节对齐，使用 memcpy 避免直接进行非对齐的 16 位访问。 */
static void cycle_loop_get_data_item(const Inv_Proto_t *protocol,
                                     rt_uint8_t data_index,
                                     Inv_RegBlk_t *reg_blk)
{
    const rt_uint8_t *data = (const rt_uint8_t *)&protocol->data;
    rt_memcpy(reg_blk,
              data + ((rt_size_t)data_index * sizeof(Inv_RegBlk_t)),
              sizeof(*reg_blk));
}

static rt_bool_t cycle_loop_select_item(void)
{
    /*
     * 外层遍历固定档案槽位，内层遍历该逆变器协议中的运行数据点。无效档案、
     * 非法 Modbus 地址、非法端口、找不到协议以及未配置的数据点都直接跳过。
     * 找到一个可读点后保存请求快照并返回，由状态机负责实际发送。
     */
    while(g_cycle_loop.archive_index < INVERTER_ARCHIVE_MAX_COUNT)
    {
        const Inv_ArchiveSlot_t *slot =
            &g_inv_archive_lib.slots[g_cycle_loop.archive_index];
        const Inv_Proto_t *protocol;

        if((slot->valid == 0U) ||
           (slot->archive.mb_addr < MODBUS_SLAVE_ADDR_MIN) ||
           (slot->archive.mb_addr > MODBUS_SLAVE_ADDR_MAX) ||
           (slot->archive.port < INV_PORT_RJ45_1) ||
           (slot->archive.port > INV_PORT_WIRELESS))
        {
            ++g_cycle_loop.archive_index;
            g_cycle_loop.data_index = 0U;
            continue;
        }

        protocol = cycle_loop_find_protocol(&slot->archive);
        if(protocol == RT_NULL)
        {
            ++g_cycle_loop.archive_index;
            g_cycle_loop.data_index = 0U;
            continue;
        }

        while(g_cycle_loop.data_index < CYCLE_LOOP_DATA_ITEM_COUNT)
        {
            Inv_RegBlk_t reg_blk;
            cycle_loop_get_data_item(protocol, g_cycle_loop.data_index, &reg_blk);
            if((reg_blk.reg_addr != INVERTER_PROTOCOL_REGISTER_UNUSED) &&
               ((reg_blk.read_func_code == MODBUS_FUNC_READ_HOLDING) ||
                 (reg_blk.read_func_code == MODBUS_FUNC_READ_INPUT)) &&
               (reg_blk.reg_cnt > 0U) &&
               (reg_blk.reg_cnt <= MODBUS_READ_REG_MAX))
            {
                g_cycle_loop.request_port = (Inv_Port_t)slot->archive.port;
                g_cycle_loop.request_reg = reg_blk;
                g_cycle_loop.request_register_count = reg_blk.reg_cnt;
                return RT_TRUE;
            }

            ++g_cycle_loop.data_index;
        }

        ++g_cycle_loop.archive_index;
        g_cycle_loop.data_index = 0U;
    }

    return RT_FALSE;
}

static rt_err_t cycle_loop_send_request(void)
{
    const Inv_Archive_t *archive =
        &g_inv_archive_lib.slots[g_cycle_loop.archive_index].archive;
    rt_uint8_t frame[MODBUS_READ_REQUEST_LEN];
    rt_uint16_t frame_len = 0U;
    rt_err_t result;

    if(g_cycle_loop.send_fn == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    /* Modbus 模块负责参数检查、字节序和 CRC，调度层不重复拼装协议字段。 */
    result = modbus_m_read_request(archive->mb_addr,
                                   g_cycle_loop.request_reg.read_func_code,
                                   g_cycle_loop.request_reg.reg_addr,
                                   g_cycle_loop.request_register_count,
                                   frame,
                                   sizeof(frame),
                                   &frame_len);
    if(result != RT_EOK)
    {
        return result;
    }

    result = g_cycle_loop.send_fn(g_cycle_loop.request_port, frame, frame_len);
    if(result == RT_EOK)
    {
        /* 只有完整报文被发送层接受后，才进入等待响应状态并开始超时计时。 */
        ++g_cycle_loop.statistics.request_count;
        g_cycle_loop.state_tick = rt_tick_get();
        g_cycle_loop.state = CYCLE_LOOP_STATE_WAIT_RESPONSE;
    }

    return result;
}

static void cycle_loop_advance_item(void)
{
    /* 成功或最终失败都移动到下一数据点；单个坏点不能阻塞整台设备或整轮扫描。 */
    ++g_cycle_loop.data_index;
    g_cycle_loop.retry_count = 0U;
    g_cycle_loop.state_tick = rt_tick_get();
    g_cycle_loop.state = CYCLE_LOOP_STATE_REQUEST_GAP;
}

static void cycle_loop_finish_cycle(rt_tick_t now)
{
    /* 周期从“本轮完成时刻”开始计算，避免一轮耗时较长时连续补发多轮请求。 */
    ++g_cycle_loop.statistics.cycle_count;
    g_cycle_loop.archive_index = 0U;
    g_cycle_loop.data_index = 0U;
    g_cycle_loop.retry_count = 0U;
    g_cycle_loop.next_cycle_tick =
        now + cycle_loop_ms_to_tick(g_cycle_loop.config.period_ms);
    g_cycle_loop.state = CYCLE_LOOP_STATE_IDLE;
}

static void cycle_loop_retry_or_advance(void)
{
    /*
     * retry_count 不包含首次发送。只要重发成功提交，就重新进入等待响应状态；
     * 所有重试都无法发送或重试次数用尽时，记录最终错误并跳过当前数据点。
     */
    while(g_cycle_loop.retry_count < g_cycle_loop.config.retry_count)
    {
        ++g_cycle_loop.retry_count;
        if(cycle_loop_send_request() == RT_EOK)
        {
            return;
        }
    }

    ++g_cycle_loop.statistics.error_count;
    cycle_loop_advance_item();
}

static void cycle_loop_handle_response(void)
{
    const Inv_Archive_t *archive =
        &g_inv_archive_lib.slots[g_cycle_loop.archive_index].archive;
    rt_uint16_t registers[MODBUS_READ_REG_MAX];
    rt_uint16_t register_count = 0U;
    rt_uint8_t exception_code = 0U;
    modbus_m_parse_result parse_result;
    rt_uint8_t frame[CYCLE_LOOP_RX_FRAME_SIZE];
    rt_uint16_t frame_len;
    Inv_Port_t frame_port;
    rt_base_t level;

    /* 在短临界区内取走单帧邮箱，随后恢复中断并在普通线程上下文中解析报文。 */
    level = rt_hw_interrupt_disable();
    frame_len = g_cycle_loop.rx_frame_len;
    frame_port = g_cycle_loop.rx_port;
    rt_memcpy(frame, g_cycle_loop.rx_frame, frame_len);
    g_cycle_loop.rx_ready = RT_FALSE;
    rt_hw_interrupt_enable(level);

    if(frame_port != g_cycle_loop.request_port)
    {
        /* 其它端口的帧不属于当前请求，忽略后继续等待，原响应超时计时不重置。 */
        return;
    }

    parse_result = modbus_m_read_response(archive->mb_addr,
                                          g_cycle_loop.request_reg.read_func_code,
                                          g_cycle_loop.request_register_count,
                                          frame,
                                          frame_len,
                                          registers,
                                          MODBUS_READ_REG_MAX,
                                          &register_count,
                                          &exception_code);
    if(parse_result != MODBUS_M_PARSE_OK)
    {
        /* CRC、地址、功能码、长度或 Modbus 异常响应均按本数据点失败处理。 */
        RT_UNUSED(exception_code);
        cycle_loop_retry_or_advance();
        return;
    }

    ++g_cycle_loop.statistics.success_count;
    if(g_cycle_loop.data_fn != RT_NULL)
    {
        /* 解析模块只转换 16 位寄存器字节序，工程量换算留给数据处理回调。 */
        g_cycle_loop.data_fn(g_cycle_loop.archive_index,
                             g_cycle_loop.data_index,
                             &g_cycle_loop.request_reg,
                             registers,
                             register_count);
    }

    cycle_loop_advance_item();
}

static void cycle_loop_process(void)
{
    rt_tick_t now = rt_tick_get();

    /*
     * 本函数每次只推进有限步骤，不执行阻塞式收帧操作。这样即使某台逆变器离线，
     * 线程仍能响应停止命令，并在超时后继续扫描其它逆变器。
     */
    switch(g_cycle_loop.state)
    {
    case CYCLE_LOOP_STATE_IDLE:
        /* 上电后的首轮 next_cycle_tick 等于当前节拍，因此可以立即开始。 */
        if(cycle_loop_tick_expired(now, g_cycle_loop.next_cycle_tick) == RT_TRUE)
        {
            g_cycle_loop.state = CYCLE_LOOP_STATE_SELECT_ITEM;
        }
        break;

    case CYCLE_LOOP_STATE_SELECT_ITEM:
        /* 没有更多有效点表示整轮结束；否则立即尝试发送选中的数据点。 */
        if(cycle_loop_select_item() == RT_FALSE)
        {
            cycle_loop_finish_cycle(now);
        }
        else if(cycle_loop_send_request() != RT_EOK)
        {
            cycle_loop_retry_or_advance();
        }
        break;

    case CYCLE_LOOP_STATE_WAIT_RESPONSE:
        /* 优先处理已经到达的帧；没有帧时再检查本次请求是否超时。 */
        if(g_cycle_loop.rx_ready == RT_TRUE)
        {
            cycle_loop_handle_response();
        }
        else if((rt_tick_t)(now - g_cycle_loop.state_tick) >=
                cycle_loop_ms_to_tick(g_cycle_loop.config.response_timeout_ms))
        {
            ++g_cycle_loop.statistics.timeout_count;
            cycle_loop_retry_or_advance();
        }
        break;

    case CYCLE_LOOP_STATE_REQUEST_GAP:
        /* 间隔到期后继续使用当前扫描游标寻找下一个有效数据点。 */
        if((rt_tick_t)(now - g_cycle_loop.state_tick) >=
           cycle_loop_ms_to_tick(g_cycle_loop.config.request_gap_ms))
        {
            g_cycle_loop.state = CYCLE_LOOP_STATE_SELECT_ITEM;
        }
        break;

    case CYCLE_LOOP_STATE_STOPPED:
    default:
        break;
    }
}

rt_err_t cycle_loop_init(const Cycle_Loop_Config_t *config,
                         cycle_loop_send_fn send_fn,
                         cycle_loop_data_fn data_fn)
{
    Cycle_Loop_Config_t effective_config;

    /* 允许调用方只使用默认配置，但发送回调必须在 start 前提供。 */
    if(config == RT_NULL)
    {
        cycle_loop_load_default_config(&effective_config);
    }
    else
    {
        effective_config = *config;
    }

    /* 零延时容易造成忙循环或连续占用总线，因此作为无效配置拒绝。 */
    if((effective_config.period_ms == 0U) ||
       (effective_config.response_timeout_ms == 0U) ||
       (effective_config.request_gap_ms == 0U))
    {
        return -RT_EINVAL;
    }

    rt_memset(&g_cycle_loop, 0, sizeof(g_cycle_loop));
    g_cycle_loop.config = effective_config;
    g_cycle_loop.send_fn = send_fn;
    g_cycle_loop.data_fn = data_fn;
    g_cycle_loop.state = CYCLE_LOOP_STATE_STOPPED;
    return RT_EOK;
}

rt_err_t cycle_loop_start(void)
{
    if(g_cycle_loop.send_fn == RT_NULL)
    {
        return -RT_ENOSYS;
    }

    /* 每次启动都从第一个档案、第一个数据点开始，但保留累计统计值。 */
    g_cycle_loop.archive_index = 0U;
    g_cycle_loop.data_index = 0U;
    g_cycle_loop.retry_count = 0U;
    g_cycle_loop.rx_ready = RT_FALSE;
    g_cycle_loop.next_cycle_tick = rt_tick_get();
    g_cycle_loop.state = CYCLE_LOOP_STATE_IDLE;
    return RT_EOK;
}

void cycle_loop_stop(void)
{
    /* 丢弃软件邮箱中的旧响应，防止再次启动后误认为是新请求的响应。 */
    g_cycle_loop.state = CYCLE_LOOP_STATE_STOPPED;
    g_cycle_loop.rx_ready = RT_FALSE;
}

rt_err_t cycle_loop_rx_frame(Inv_Port_t port,
                             const rt_uint8_t *frame,
                             rt_uint16_t frame_len)
{
    rt_base_t level;

    if((frame == RT_NULL) || (frame_len == 0U) ||
       (frame_len > CYCLE_LOOP_RX_FRAME_SIZE))
    {
        return -RT_EINVAL;
    }

    /* 非等待状态没有请求可与该帧对应，交由其它协议模块处理或直接丢弃。 */
    if(g_cycle_loop.state != CYCLE_LOOP_STATE_WAIT_RESPONSE)
    {
        return -RT_EBUSY;
    }

    /* 单帧邮箱不覆盖未处理数据，避免响应帧在周期线程读取前被下一帧破坏。 */
    level = rt_hw_interrupt_disable();
    if(g_cycle_loop.rx_ready == RT_TRUE)
    {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;
    }

    rt_memcpy(g_cycle_loop.rx_frame, frame, frame_len);
    g_cycle_loop.rx_frame_len = frame_len;
    g_cycle_loop.rx_port = port;
    g_cycle_loop.rx_ready = RT_TRUE;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

Cycle_Loop_State_t cycle_loop_get_state(void)
{
    return g_cycle_loop.state;
}

void cycle_loop_get_statistics(Cycle_Loop_Statistics_t *statistics)
{
    if(statistics != RT_NULL)
    {
        rt_base_t level = rt_hw_interrupt_disable();
        *statistics = g_cycle_loop.statistics;
        rt_hw_interrupt_enable(level);
    }
}

void cycle_loop_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while(1)
    {
        /* 固定短周期推进状态机；具体抄读周期由 next_cycle_tick 单独控制。 */
        cycle_loop_process();
        rt_thread_mdelay(CYCLE_LOOP_THREAD_POLL_MS);
    }
}
