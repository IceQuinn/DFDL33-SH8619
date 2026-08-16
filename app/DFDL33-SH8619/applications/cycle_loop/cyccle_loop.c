/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cycle_loop.h"

#include <rthw.h>

#include "inverter_protocol_library.h"
#include "modbus_master.h"
#include "uart_def.h"
#include "main_uart.h"

#define CYCLE_LOOP_THREAD_POLL_MS          10U
#define CYCLE_LOOP_RESPONSE_TIMEOUT_TICKS 1000U
#define CYCLE_LOOP_SCAN_PORT_COUNT          3U
#define CYCLE_LOOP_SCAN_ADDR_FIRST          1U
#define CYCLE_LOOP_SCAN_ADDR_LAST          10U
#define CYCLE_LOOP_PROBE_REG_ADDR            0U
#define CYCLE_LOOP_PROBE_REG_COUNT           1U
#define CYCLE_LOOP_RX_FRAME_SIZE            MODBUS_RTU_ADU_MAX

/* 搜索分为固定探测和协议识别两个阶段，只有固定探测收到回复后才进入协议识别。 */
typedef enum Cycle_Loop_Scan_Phase
{
    CYCLE_LOOP_PHASE_PROBE = 0,         /* 使用03功能码、寄存器地址0、数量1搜索从站地址。 */
    CYCLE_LOOP_PHASE_FEATURE            /* 使用有效协议中的feature配置识别设备协议。 */
} Cycle_Loop_Scan_Phase_t;

/* 每个串口独立运行状态机，等待某个串口响应时仍可继续推进另外两个串口。 */
typedef enum Cycle_Loop_Scan_State
{
    CYCLE_LOOP_SCAN_READY = 0,          /* 当前串口可以组成并打印下一帧请求。 */
    CYCLE_LOOP_SCAN_WAIT_RESPONSE,      /* 请求已经打印，等待接收帧或者1秒超时。 */
    CYCLE_LOOP_SCAN_STOPPED             /* 地址或协议扫描结束，不再打印该串口请求。 */
} Cycle_Loop_Scan_State_t;

/* 单个串口的扫描上下文，三个实例分别保存扫描游标、超时节拍和接收邮箱。 */
typedef struct Cycle_Loop_Uart_Context
{
    uint16_t uart_no;                   /* 串口管理层使用的通道下标，例如UART1_NO。 */
    Cycle_Loop_Scan_Phase_t phase;      /* 当前处于固定探测阶段还是协议识别阶段。 */
    Cycle_Loop_Scan_State_t state;      /* 当前串口状态机状态。 */
    uint16_t protocol_index;            /* 协议识别阶段正在测试的协议库下标。 */
    uint16_t feature_reg_addr;          /* 当前协议的特征寄存器起始地址。 */
    uint16_t feature_default_val;       /* 用于判断协议是否匹配的默认特征值。 */
    uint8_t slave_addr;                 /* 当前设备的Modbus地址，搜索范围为1～10。 */
    uint8_t feature_reg_cnt;            /* 当前协议特征数据占用的寄存器数量。 */
    uint8_t feature_func_code;          /* 当前协议特征寄存器使用的读功能码。 */
    rt_tick_t request_tick;             /* 最近一次打印请求报文时的系统tick。 */

    uint8_t rx_frame[CYCLE_LOOP_RX_FRAME_SIZE]; /* 通信接收层提交的完整Modbus响应帧。 */
    uint16_t rx_frame_len;              /* 接收邮箱中响应帧的有效字节数。 */
    volatile rt_bool_t rx_ready;        /* RT_TRUE表示邮箱中存在待处理响应帧。 */
} Cycle_Loop_Uart_Context_t;

/* 三个串口分别使用独立上下文，允许它们同时处于等待响应状态。 */
static Cycle_Loop_Uart_Context_t g_scan_uarts[CYCLE_LOOP_SCAN_PORT_COUNT];

/* 自动搜索只扫描三个有线串口，不扫描无线端口。 */
static const uint16_t g_scan_uart_list[CYCLE_LOOP_SCAN_PORT_COUNT] =
{
    UART1_NO,
    UART3_NO,
    UART5_NO,
};

/* 根据接收帧携带的端口号查找对应上下文，非扫描端口返回RT_NULL。 */
static Cycle_Loop_Uart_Context_t *cycle_loop_find_uart(uint16_t uart_no)
{
    uint8_t index;

    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index)
    {
        if(g_scan_uarts[index].uart_no == uart_no)
        {
            return &g_scan_uarts[index];
        }
    }

    return RT_NULL;
}

/* 打印一帧完整Modbus RTU报文，行首tick只读取一次，后续字节属于同一日志行。 */
static void cycle_loop_print_frame(const Cycle_Loop_Uart_Context_t *context,
                                   const char *request_type,
                                   const uint8_t *frame,
                                   uint16_t frame_len)
{
    uint16_t byte_index;

    rt_kprintf("[%08d] uart[%u] %s addr[%u] request:",
               (int)rt_tick_get(),
               (unsigned int)context->uart_no,
               request_type,
               (unsigned int)context->slave_addr);
    for(byte_index = 0U; byte_index < frame_len; ++byte_index)
    {
        rt_kprintf(" %02X", (unsigned int)frame[byte_index]);
    }
    rt_kprintf("\n");
}

/* 停止指定串口的搜索并清空该串口尚未处理的旧响应。 */
static void cycle_loop_stop_uart(Cycle_Loop_Uart_Context_t *context)
{
    context->state = CYCLE_LOOP_SCAN_STOPPED;
    context->rx_ready = RT_FALSE;
    rt_kprintf("[%08d] uart[%u] discovery stopped\n",
               (int)rt_tick_get(),
               (unsigned int)context->uart_no);
}

/* 打印动作视为模拟发送成功，从打印完成时开始独立计算该串口的1秒超时。 */
static void cycle_loop_start_wait(Cycle_Loop_Uart_Context_t *context)
{
    context->request_tick = rt_tick_get();
    context->state = CYCLE_LOOP_SCAN_WAIT_RESPONSE;
}

/* 固定探测超时后增加Modbus地址，地址10仍超时则停止该串口。 */
static void cycle_loop_advance_probe_addr(Cycle_Loop_Uart_Context_t *context)
{
    if(context->slave_addr < CYCLE_LOOP_SCAN_ADDR_LAST)
    {
        ++context->slave_addr;
        context->state = CYCLE_LOOP_SCAN_READY;
    }
    else
    {
        cycle_loop_stop_uart(context);
    }
}

/* 当前特征请求未识别到协议时切换下一条协议，从站地址保持为固定探测响应地址。 */
static void cycle_loop_advance_protocol(Cycle_Loop_Uart_Context_t *context)
{
    ++context->protocol_index;
    context->state = CYCLE_LOOP_SCAN_READY;
}

/* 使用固定参数组成探测请求：03功能码、寄存器地址0、读取寄存器数量1。 */
static void cycle_loop_print_probe_request(Cycle_Loop_Uart_Context_t *context)
{
    uint8_t frame[MODBUS_READ_REQUEST_LEN];
    uint16_t frame_len = 0U;

    if(modbus_m_read_request(context->slave_addr,
                             MODBUS_FUNC_READ_HOLDING,
                             CYCLE_LOOP_PROBE_REG_ADDR,
                             CYCLE_LOOP_PROBE_REG_COUNT,
                             frame,
                             sizeof(frame),
                             &frame_len) != RT_EOK)
    {
        rt_kprintf("[%08d] uart[%u] addr[%u] probe request build failed\n",
                   (int)rt_tick_get(),
                   (unsigned int)context->uart_no,
                   (unsigned int)context->slave_addr);
        cycle_loop_stop_uart(context);
        return;
    }

    /* 这里只打印报文模拟串口发送，不调用任何真实串口发送接口。 */
    cycle_loop_print_frame(context, "probe", frame, frame_len);
    uart_mgmt_write(context->uart_no, frame, frame_len);
    cycle_loop_start_wait(context);
}

/* 选择下一条有效协议，根据其特征寄存器组成报文并打印。 */
static void cycle_loop_print_feature_request(Cycle_Loop_Uart_Context_t *context)
{
    while(context->protocol_index < INVERTER_PROTOCOL_LIBRARY_COUNT)
    {
        const Inv_Proto_t *protocol;
        Inv_Feature_t feature;
        uint8_t frame[MODBUS_READ_REQUEST_LEN];
        uint16_t frame_len = 0U;

        if(g_inv_proto_lib.valid[context->protocol_index] !=
           INVERTER_PROTOCOL_VALID)
        {
            ++context->protocol_index;  /* 无效协议不组帧，直接查找下一条协议。 */
            continue;
        }

        protocol = &g_inv_proto_lib.proto[context->protocol_index];
        /* Inv_Proto_t按1字节对齐，先复制feature再读取其中的16位字段。 */
        rt_memcpy(&feature, &protocol->feature, sizeof(feature));
        context->feature_reg_addr = feature.reg_addr;
        context->feature_default_val = feature.default_val;
        context->feature_reg_cnt = feature.reg_cnt;
        context->feature_func_code = feature.read_func_code;

        if(modbus_m_read_request(context->slave_addr,
                                 context->feature_func_code,
                                 context->feature_reg_addr,
                                 context->feature_reg_cnt,
                                 frame,
                                 sizeof(frame),
                                 &frame_len) != RT_EOK)
        {
            rt_kprintf("[%08d] uart[%u] protocol[%u] feature register is invalid\n",
                       (int)rt_tick_get(),
                       (unsigned int)context->uart_no,
                       (unsigned int)context->protocol_index);
            ++context->protocol_index;
            continue;
        }

        rt_kprintf("[%08d] uart[%u] protocol[%u] addr[%u] feature request:",
                   (int)rt_tick_get(),
                   (unsigned int)context->uart_no,
                   (unsigned int)context->protocol_index,
                   (unsigned int)context->slave_addr);
        for(uint16_t byte_index = 0U; byte_index < frame_len; ++byte_index)
        {
            rt_kprintf(" %02X", (unsigned int)frame[byte_index]);
        }
        rt_kprintf("\n");
        cycle_loop_start_wait(context);
        return;
    }

    cycle_loop_stop_uart(context);       /* 所有有效协议均已测试，结束该串口搜索。 */
}

/* 从接收邮箱安全取出一帧，复制完成后立即释放邮箱供接收线程继续使用。 */
static uint16_t cycle_loop_take_rx_frame(Cycle_Loop_Uart_Context_t *context,
                                         uint8_t *frame)
{
    uint16_t frame_len;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    frame_len = context->rx_frame_len;
    rt_memcpy(frame, context->rx_frame, frame_len);
    context->rx_ready = RT_FALSE;
    rt_hw_interrupt_enable(level);
    return frame_len;
}

/* 固定探测收到合法响应后锁定当前地址，并从协议库第一条开始识别协议。 */
static void cycle_loop_handle_probe_response(Cycle_Loop_Uart_Context_t *context,
                                             const uint8_t *frame,
                                             uint16_t frame_len)
{
    uint16_t registers[CYCLE_LOOP_PROBE_REG_COUNT];
    uint16_t register_count = 0U;
    uint8_t exception_code = 0U;
    modbus_m_parse_result result;

    result = modbus_m_read_response(context->slave_addr,
                                    MODBUS_FUNC_READ_HOLDING,
                                    CYCLE_LOOP_PROBE_REG_COUNT,
                                    frame,
                                    frame_len,
                                    registers,
                                    CYCLE_LOOP_PROBE_REG_COUNT,
                                    &register_count,
                                    &exception_code);
    if(result != MODBUS_M_PARSE_OK)
    {
        RT_UNUSED(exception_code);
        rt_kprintf("[%08d] uart[%u] ignored probe response, parse result[%u]\n",
                   (int)rt_tick_get(),
                   (unsigned int)context->uart_no,
                   (unsigned int)result);
        return;                         /* 错误或无关报文不重置原1秒计时。 */
    }

    RT_UNUSED(registers);
    RT_UNUSED(register_count);
    rt_kprintf("[%08d] uart[%u] addr[%u] probe response received\n",
               (int)rt_tick_get(),
               (unsigned int)context->uart_no,
               (unsigned int)context->slave_addr);

    context->phase = CYCLE_LOOP_PHASE_FEATURE;
    context->protocol_index = 0U;
    context->state = CYCLE_LOOP_SCAN_READY;
}

/* 协议特征响应通过Modbus校验后，再比较首个寄存器与协议默认特征值。 */
static void cycle_loop_handle_feature_response(Cycle_Loop_Uart_Context_t *context,
                                               const uint8_t *frame,
                                               uint16_t frame_len)
{
    uint16_t registers[MODBUS_READ_REG_MAX];
    uint16_t register_count = 0U;
    uint8_t exception_code = 0U;
    modbus_m_parse_result result;

    result = modbus_m_read_response(context->slave_addr,
                                    context->feature_func_code,
                                    context->feature_reg_cnt,
                                    frame,
                                    frame_len,
                                    registers,
                                    MODBUS_READ_REG_MAX,
                                    &register_count,
                                    &exception_code);
    if(result != MODBUS_M_PARSE_OK)
    {
        RT_UNUSED(exception_code);
        rt_kprintf("[%08d] uart[%u] ignored feature response, parse result[%u]\n",
                   (int)rt_tick_get(),
                   (unsigned int)context->uart_no,
                   (unsigned int)result);
        return;                         /* 错误或无关报文不重置原1秒计时。 */
    }

    if((register_count > 0U) &&
       (registers[0] == context->feature_default_val))
    {
        rt_kprintf("[%08d] uart[%u] found protocol[%u] addr[%u], feature[0x%04X]\n",
                   (int)rt_tick_get(),
                   (unsigned int)context->uart_no,
                   (unsigned int)context->protocol_index,
                   (unsigned int)context->slave_addr,
                   (unsigned int)registers[0]);
        cycle_loop_stop_uart(context);
        return;
    }

    rt_kprintf("[%08d] uart[%u] protocol[%u] feature mismatch\n",
               (int)rt_tick_get(),
               (unsigned int)context->uart_no,
               (unsigned int)context->protocol_index);
    cycle_loop_advance_protocol(context); /* 合法响应但特征不匹配时立即测试下一条协议。 */
}

/* 根据当前阶段把邮箱中的响应交给固定探测解析器或协议特征解析器。 */
static void cycle_loop_handle_response(Cycle_Loop_Uart_Context_t *context)
{
    uint8_t frame[CYCLE_LOOP_RX_FRAME_SIZE];
    uint16_t frame_len = cycle_loop_take_rx_frame(context, frame);

    if(context->phase == CYCLE_LOOP_PHASE_PROBE)
    {
        cycle_loop_handle_probe_response(context, frame, frame_len);
    }
    else
    {
        cycle_loop_handle_feature_response(context, frame, frame_len);
    }
}

/* 每次最多推进一个状态，保证一个串口等待响应时另外两个串口仍能继续打印。 */
static void cycle_loop_process_uart(Cycle_Loop_Uart_Context_t *context,
                                    rt_tick_t now)
{
    switch(context->state)
    {
    case CYCLE_LOOP_SCAN_READY:
        if(context->phase == CYCLE_LOOP_PHASE_PROBE)
        {
            cycle_loop_print_probe_request(context);
        }
        else
        {
            cycle_loop_print_feature_request(context);
        }
        break;

    case CYCLE_LOOP_SCAN_WAIT_RESPONSE:
        if(context->rx_ready == RT_TRUE)
        {
            cycle_loop_handle_response(context); /* 响应优先于同一tick发生的超时。 */
        }
        else if((rt_tick_t)(now - context->request_tick) >=
                CYCLE_LOOP_RESPONSE_TIMEOUT_TICKS)
        {
            if(context->phase == CYCLE_LOOP_PHASE_PROBE)
            {
                rt_kprintf("[%08d] uart[%u] addr[%u] probe response timeout\n",
                           (int)rt_tick_get(),
                           (unsigned int)context->uart_no,
                           (unsigned int)context->slave_addr);
                cycle_loop_advance_probe_addr(context);
            }
            else
            {
                rt_kprintf("[%08d] uart[%u] protocol[%u] feature response timeout\n",
                           (int)rt_tick_get(),
                           (unsigned int)context->uart_no,
                           (unsigned int)context->protocol_index);
                cycle_loop_advance_protocol(context);
            }
        }
        break;

    case CYCLE_LOOP_SCAN_STOPPED:
    default:
        break;
    }
}

/* 三个串口均从固定探测阶段、Modbus地址1开始，协议库此时不会被访问。 */
static void cycle_loop_init_scan_uarts(void)
{
    uint8_t index;

    rt_memset(g_scan_uarts, 0, sizeof(g_scan_uarts));
    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index)
    {
        g_scan_uarts[index].uart_no = g_scan_uart_list[index];
        g_scan_uarts[index].phase = CYCLE_LOOP_PHASE_PROBE;
        g_scan_uarts[index].slave_addr = CYCLE_LOOP_SCAN_ADDR_FIRST;
        g_scan_uarts[index].state = CYCLE_LOOP_SCAN_READY;
    }
}

rt_err_t cycle_loop_rx_frame(uint16_t uart_no,
                             const uint8_t *frame,
                             uint16_t frame_len)
{
    Cycle_Loop_Uart_Context_t *context;
    rt_base_t level;

    if((frame == RT_NULL) || (frame_len == 0U) ||
       (frame_len > CYCLE_LOOP_RX_FRAME_SIZE))
    {
        return -RT_EINVAL;
    }

    context = cycle_loop_find_uart(uart_no);
    if((context == RT_NULL) ||
       (context->state != CYCLE_LOOP_SCAN_WAIT_RESPONSE))
    {
        return -RT_EBUSY;               /* 非扫描端口或非等待状态不接收响应。 */
    }

    level = rt_hw_interrupt_disable();
    if(context->rx_ready == RT_TRUE)
    {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;               /* 旧响应未处理时禁止覆盖单帧邮箱。 */
    }

    rt_memcpy(context->rx_frame, frame, frame_len);
    context->rx_frame_len = frame_len;
    context->rx_ready = RT_TRUE;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

void cycle_loop_thread_entry(void *parameter)
{
    uint8_t index;

    RT_UNUSED(parameter);

    if(g_inv_archive_lib.count != 0U)
    {
        rt_kprintf("[%08d] archive count[%u], discovery is not required\n", rt_tick_get(), g_inv_archive_lib.count);
        return;                         /* 当前需求不再执行已有档案的周期抄读。 */
    }

    cycle_loop_init_scan_uarts();
    while(1)
    {
        rt_tick_t now = rt_tick_get();

        /* 顺序推进三个独立状态机，不会等待前一个串口超时后才处理下一个串口。 */
        for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index)
        {
            cycle_loop_process_uart(&g_scan_uarts[index], now);
        }

        rt_thread_mdelay(CYCLE_LOOP_THREAD_POLL_MS);
    }
}
