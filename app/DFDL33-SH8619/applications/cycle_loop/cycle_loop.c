/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cycle_loop.h"

#include <rthw.h>

#include "inverter_archive.h"
#include "inverter_protocol_library.h"
#include "inv_data.h"
#include "modbus_master.h"
#include "uart_def.h"
#include "main_uart.h"
#include "sys.h"
#include "user_rtc.h"

#define CYCLE_LOOP_THREAD_POLL_MS          10U
#define CYCLE_LOOP_RESPONSE_TIMEOUT_TICKS 1000U
#define CYCLE_LOOP_SCAN_PORT_COUNT          3U
#define CYCLE_LOOP_SCAN_ADDR_FIRST          1U
#define CYCLE_LOOP_SCAN_ADDR_LAST          10U
#define CYCLE_LOOP_PROBE_REG_ADDR            0U
#define CYCLE_LOOP_PROBE_REG_COUNT           1U
#define CYCLE_LOOP_RX_FRAME_SIZE            MODBUS_RTU_ADU_MAX

/* 搜索分为固定探测和协议识别两个阶段，只有固定探测收到回复后才进入协议识别。 */
typedef enum Cycle_Loop_Scan_Phase {
    CYCLE_LOOP_PHASE_PROBE = 0,         /* 使用03功能码、寄存器地址0、数量1搜索从站地址。 */
    CYCLE_LOOP_PHASE_FEATURE            /* 使用有效协议中的feature配置识别设备协议。 */
} Cycle_Loop_Scan_Phase_t;

/* 每个串口独立运行状态机，等待某个串口响应时仍可继续推进另外两个串口。 */
typedef enum Cycle_Loop_Scan_State {
    CYCLE_LOOP_SCAN_READY = 0,          /* 当前串口可以组成并发送下一帧请求。 */
    CYCLE_LOOP_SCAN_WAIT_RESPONSE,      /* 请求已经完整写入串口，等待接收帧或者1秒超时。 */
    CYCLE_LOOP_SCAN_STOPPED             /* 地址或协议扫描结束，不再发送该串口请求。 */
} Cycle_Loop_Scan_State_t;

/* 单个串口的扫描上下文，三个实例分别保存扫描游标、超时节拍和接收邮箱。 */
typedef struct Cycle_Loop_Uart_Context {
    uint16_t uart_no;                   /* 串口管理层使用的通道下标，例如UART1_NO。 */
    Cycle_Loop_Scan_Phase_t phase;      /* 当前处于固定探测阶段还是协议识别阶段。 */
    Cycle_Loop_Scan_State_t state;      /* 当前串口状态机状态。 */
    uint16_t protocol_index;            /* 协议识别阶段正在测试的协议库下标。 */
    uint16_t feature_reg_addr;          /* 当前协议的特征寄存器起始地址。 */
    uint16_t feature_default_val;       /* 用于判断协议是否匹配的默认特征值。 */
    uint8_t slave_addr;                 /* 当前设备的Modbus地址，搜索范围为1～10。 */
    uint8_t feature_reg_cnt;            /* 当前协议特征数据占用的寄存器数量。 */
    uint8_t feature_func_code;          /* 当前协议特征寄存器使用的读功能码。 */
    rt_tick_t request_tick;             /* 最近一次请求完整写入串口时的系统tick。 */

    uint8_t rx_frame[CYCLE_LOOP_RX_FRAME_SIZE]; /* 通信接收层提交的完整Modbus响应帧。 */
    uint16_t rx_frame_len;              /* 接收邮箱中响应帧的有效字节数。 */
    volatile rt_bool_t rx_ready;        /* RT_TRUE表示邮箱中存在待处理响应帧。 */
} Cycle_Loop_Uart_Context_t;

/* 一项映射同时保存串口管理层编号和档案协议端口号，两套编号不要求数值相同。 */
typedef struct Cycle_Loop_Port_Map {
    uint16_t uart_no;
    uint8_t archive_port;
} Cycle_Loop_Port_Map_t;

/* 三个串口分别使用独立上下文，允许它们同时处于等待响应状态。 */
static Cycle_Loop_Uart_Context_t g_scan_uarts[CYCLE_LOOP_SCAN_PORT_COUNT];

/* 自动搜索使用固定映射表，扫描取串口号，识别成功后再按串口号查询档案端口。 */
static const Cycle_Loop_Port_Map_t g_scan_port_map[CYCLE_LOOP_SCAN_PORT_COUNT] = {
    {UART6_NO, INV_PORT_RS485_2},       /* UART1连接RS485-II。 */
    {UART7_NO, INV_PORT_RJ45_1},        /* UART3连接RJ45-I。 */
    {UART4_NO, INV_PORT_RJ45_2},        /* UART5连接RJ45-II。 */
};

/* 根据接收帧携带的端口号查找对应上下文，非扫描端口返回RT_NULL。 */
static Cycle_Loop_Uart_Context_t *cycle_loop_find_uart(uint16_t uart_no)
{
    uint8_t index;

    /* 遍历三个扫描上下文，查找串口编号相同的上下文。 */
    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index) {
        /* 串口编号匹配时立即返回对应上下文。 */
        if(g_scan_uarts[index].uart_no == uart_no) {
            return &g_scan_uarts[index];
        }
    }

    return RT_NULL;
}

/* 按串口号查询映射表中的档案端口，未配置的串口或空输出指针返回RT_FALSE。 */
static rt_bool_t cycle_loop_uart_to_archive_port(uint16_t uart_no,
                                                 uint8_t *archive_port)
{
    uint8_t index;

    /* 输出指针为空时无法返回映射后的档案端口。 */
    if(archive_port == RT_NULL) {
        return RT_FALSE;
    }

    /* 遍历固定映射表，查找串口编号对应的档案端口。 */
    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index) {
        /* 找到串口编号后写入档案端口并结束查询。 */
        if(g_scan_port_map[index].uart_no == uart_no) {
            *archive_port = g_scan_port_map[index].archive_port;
            return RT_TRUE;
        }
    }

    return RT_FALSE;
}

/* 打印一帧完整Modbus RTU报文，行首时间只读取一次，后续字节属于同一日志行。 */
static void cycle_loop_print_frame(const Cycle_Loop_Uart_Context_t *context,
                                   const char *request_type,
                                   const uint8_t *frame,
                                   uint16_t frame_len)
{
    char frame_name[64];

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] %s addr[%d] request", context->uart_no, request_type, context->slave_addr);
    show_arr(frame_name, frame, frame_len);
}

/* 解析完成后打印原始响应和解析结果，校验失败时也保留完整报文用于定位问题。 */
static void cycle_loop_print_response(const Cycle_Loop_Uart_Context_t *context,
                                      const char *response_type,
                                      modbus_m_parse_result parse_result,
                                      const uint8_t *frame,
                                      uint16_t frame_len)
{
    char frame_name[80];

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] %s addr[%d] reply[%s]", context->uart_no, response_type, context->slave_addr, modbus_m_parse_result_text(parse_result));
    show_arr(frame_name, frame, frame_len);
}

/* 停止指定串口的搜索并清空该串口尚未处理的旧响应。 */
static void cycle_loop_stop_uart(Cycle_Loop_Uart_Context_t *context)
{
    context->state = CYCLE_LOOP_SCAN_STOPPED;
    context->rx_ready = RT_FALSE;
    rt_kprintf("%s uart[%d] device scan stopped\n", get_char_time(), context->uart_no);
}

/* 只有完整报文写入串口后才进入等待状态，避免发送失败也被误判成接收超时。 */
static void cycle_loop_start_wait(Cycle_Loop_Uart_Context_t *context)
{
    context->request_tick = rt_tick_get();
    context->state = CYCLE_LOOP_SCAN_WAIT_RESPONSE;
}

/* 所有请求统一在这里打印并发送，完整写入后才启动该串口独立的1秒响应计时。 */
static rt_bool_t cycle_loop_send_frame(Cycle_Loop_Uart_Context_t *context,
                                       const char *request_type,
                                       const uint8_t *frame,
                                       uint16_t frame_len)
{
    rt_size_t written_size;

    cycle_loop_print_frame(context, request_type, frame, frame_len);
    written_size = uart_mgmt_write(context->uart_no, frame, frame_len);

    /* 实际写入长度与报文长度不一致时，当前串口发送失败。 */
    if(written_size != frame_len) {
        rt_kprintf("%s uart[%d] could not send full %s request, expected[%d], sent[%d]\n", get_char_time(), context->uart_no, request_type, frame_len, written_size);
        cycle_loop_stop_uart(context);  /* 串口不可用时停止本端口，另外两个端口继续运行。 */
        return RT_FALSE;
    }

    cycle_loop_start_wait(context);
    return RT_TRUE;
}

/* 固定探测超时后增加Modbus地址，地址10仍超时则停止该串口。 */
static void cycle_loop_advance_probe_addr(Cycle_Loop_Uart_Context_t *context)
{
    /* 当前地址没有达到10时切换到下一个Modbus地址继续探测。 */
    if(context->slave_addr < CYCLE_LOOP_SCAN_ADDR_LAST) {
        ++context->slave_addr;
        /* READY使下一次10ms调度立即按新地址重新组帧，不在本次超时处理中直接发送。 */
        context->state = CYCLE_LOOP_SCAN_READY;
    }
    /* 地址10仍未收到响应时结束当前串口的探测。 */
    else {
        cycle_loop_stop_uart(context);
    }
}

/* 当前特征请求未识别到协议时切换下一条协议，从站地址保持为固定探测响应地址。 */
static void cycle_loop_advance_protocol(Cycle_Loop_Uart_Context_t *context)
{
    ++context->protocol_index;
    /* 从站地址保持不变，下一次调度只切换协议库中的特征寄存器配置。 */
    context->state = CYCLE_LOOP_SCAN_READY;
}

/* 实际值位于默认值的90%～110%闭区间时匹配，放大100倍比较可避免浮点和取整误差。 */
static rt_bool_t cycle_loop_feature_value_matches(uint16_t actual_value,
                                                  uint16_t default_value)
{
    uint32_t actual_scaled = (uint32_t)actual_value * 100U;
    uint32_t lower_scaled = (uint32_t)default_value * 90U;
    uint32_t upper_scaled = (uint32_t)default_value * 110U;

    return ((actual_scaled >= lower_scaled) &&
            (actual_scaled <= upper_scaled)) ? RT_TRUE : RT_FALSE;
}

/* 使用匹配协议的厂家信息、当前从站地址和物理接入端口生成并持久化档案。 */
static rt_bool_t cycle_loop_add_matched_archive(const Cycle_Loop_Uart_Context_t *context)
{
    const Inv_Proto_t *protocol;
    uint8_t archive_port;
    int8_t archive_index;

    /* 协议下标越界或串口无法映射到档案端口时不能生成档案。 */
    if((context->protocol_index >= INVERTER_PROTOCOL_LIBRARY_COUNT) ||
       (cycle_loop_uart_to_archive_port(context->uart_no, &archive_port) == RT_FALSE)) {
        rt_kprintf("%s uart[%d] archive port was not found\n", get_char_time(), context->uart_no);
        return RT_FALSE;
    }

    protocol = &g_inv_proto_lib.proto[context->protocol_index];
    archive_index = Inv_Archive_Add_Device(context->slave_addr,
                                           archive_port,
                                           &protocol->mfr_info);

    /* 档案库已满或档案参数无效时报告新增失败。 */
    if(archive_index == INVERTER_ARCHIVE_ADD_FAILED) {
        rt_kprintf("%s uart[%d] addr[%d] could not be added to archive\n", get_char_time(), context->uart_no, context->slave_addr);
        return RT_FALSE;
    }

    rt_kprintf("%s uart[%d] addr[%d] added to archive[%d], port[%d]\n", get_char_time(), context->uart_no, context->slave_addr, archive_index + 1, archive_port);
    return RT_TRUE;
}

/* 使用固定参数组成探测请求：03功能码、寄存器地址0、读取寄存器数量1。 */
static void cycle_loop_send_probe_request(Cycle_Loop_Uart_Context_t *context)
{
    uint8_t frame[MODBUS_READ_REQUEST_LEN];
    uint16_t frame_len = 0U;

    /* 固定探测请求组帧失败时停止当前串口，避免持续重复使用错误参数。 */
    if(modbus_m_read_request(context->slave_addr,
                             MODBUS_FUNC_READ_HOLDING,
                             CYCLE_LOOP_PROBE_REG_ADDR,
                             CYCLE_LOOP_PROBE_REG_COUNT,
                             frame,
                             sizeof(frame),
                             &frame_len) != RT_EOK) {
        rt_kprintf("%s uart[%d] addr[%d] could not build address probe request\n", get_char_time(), context->uart_no, context->slave_addr);
        cycle_loop_stop_uart(context);
        return;
    }
    // 发送报文
    cycle_loop_send_frame(context, "probe", frame, frame_len);
}

/* 选择下一条有效协议，根据其特征寄存器组成报文并发送。 */
static void cycle_loop_send_feature_request(Cycle_Loop_Uart_Context_t *context)
{
    /* 从当前协议下标开始查找下一条可以组成特征请求的有效协议。 */
    while(context->protocol_index < INVERTER_PROTOCOL_LIBRARY_COUNT) {
        const Inv_Proto_t *protocol;
        Inv_Feature_t feature;
        uint8_t frame[MODBUS_READ_REQUEST_LEN];
        uint16_t frame_len = 0U;

        /* 无效协议不组帧，直接移动到下一条协议。 */
        if(g_inv_proto_lib.valid[context->protocol_index] !=
           INVERTER_PROTOCOL_VALID) {
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

        /* 特征寄存器配置无效时跳过当前协议，继续检查下一条协议。 */
        if(modbus_m_read_request(context->slave_addr,
                                 context->feature_func_code,
                                 context->feature_reg_addr,
                                 context->feature_reg_cnt,
                                 frame,
                                 sizeof(frame),
                                 &frame_len) != RT_EOK) {
            rt_kprintf("%s uart[%d] protocol[%d] has an invalid feature register\n", get_char_time(), context->uart_no, context->protocol_index);
            ++context->protocol_index;
            continue;
        }

        /* 一次只发送一个协议的特征请求，响应或超时后再切换protocol_index。 */
        cycle_loop_send_frame(context, "feature", frame, frame_len);
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
    cycle_loop_print_response(context, "probe", result, frame, frame_len);

    /* 正常响应通过全部校验时，确认当前Modbus地址存在设备。 */
    if(result == MODBUS_M_PARSE_OK) {
        rt_kprintf("%s uart[%d] addr[%d] replied to address probe\n", get_char_time(), context->uart_no, context->slave_addr);
    }
    /* CRC、地址及异常帧长度均正确的非法数据地址响应同样证明设备在线。 */
    else if((result == MODBUS_M_PARSE_EXCEPTION) &&
            (exception_code == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS)) {
        rt_kprintf("%s uart[%d] addr[%d] returned exception[%d], communication is working\n", get_char_time(), context->uart_no, context->slave_addr, exception_code);
    }
    /* 其他解析结果不能证明当前地址通信成功，立即切换下一地址。 */
    else {
        rt_kprintf("%s uart[%d] address probe reply rejected: %s, exception[%d]\n", get_char_time(), context->uart_no, modbus_m_parse_result_text(result), exception_code);
        /* 已收到一帧说明本次等待已经结束，解析失败按探测失败处理并立即尝试下一地址。 */
        cycle_loop_advance_probe_addr(context);
        return;
    }

    /* 固定探测只用于确认设备在线，协议识别必须从协议库下标0重新开始。 */
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
    cycle_loop_print_response(context, "feature", result, frame, frame_len);

    /* 特征响应解析失败时不再等待超时，直接切换下一条有效协议。 */
    if(result != MODBUS_M_PARSE_OK) {
        rt_kprintf("%s uart[%d] feature reply rejected: %s, exception[%d]\n", get_char_time(), context->uart_no, modbus_m_parse_result_text(result), exception_code);
        /* 已收到一帧后不再等待当前协议超时，解析失败直接查询下一条有效协议。 */
        cycle_loop_advance_protocol(context);
        return;
    }

    /* 至少解析出一个寄存器并且首个值位于默认值正负10%范围时，协议匹配成功。 */
    if((register_count > 0U) &&
       (cycle_loop_feature_value_matches(registers[0],
                                         context->feature_default_val) == RT_TRUE)) {
        rt_kprintf("%s uart[%d] addr[%d] matched protocol[%d], feature[%d], expected[%d]\n", get_char_time(), context->uart_no, context->slave_addr, context->protocol_index, registers[0], context->feature_default_val);
        /* 识别成功后立即写入档案并保存Flash，无论保存结果如何都结束本串口搜索。 */
        cycle_loop_add_matched_archive(context);
        cycle_loop_stop_uart(context);
        return;
    }

    rt_kprintf("%s uart[%d] protocol[%d] feature[%d] is not within 10%% of expected[%d]\n", get_char_time(), context->uart_no, context->protocol_index, (register_count > 0U) ? registers[0] : 0U, context->feature_default_val);
    cycle_loop_advance_protocol(context); /* 合法响应但特征不匹配时立即测试下一条协议。 */
}

/* 根据当前阶段把邮箱中的响应交给固定探测解析器或协议特征解析器。 */
static void cycle_loop_handle_response(Cycle_Loop_Uart_Context_t *context)
{
    uint8_t frame[CYCLE_LOOP_RX_FRAME_SIZE];
    uint16_t frame_len = cycle_loop_take_rx_frame(context, frame);

    /* 固定探测阶段使用固定03功能码响应解析逻辑。 */
    if(context->phase == CYCLE_LOOP_PHASE_PROBE) {
        cycle_loop_handle_probe_response(context, frame, frame_len);
    }
    /* 特征识别阶段使用当前协议的特征响应解析逻辑。 */
    else {
        cycle_loop_handle_feature_response(context, frame, frame_len);
    }
}

/* 每次最多推进一个状态，保证一个串口等待响应时另外两个串口仍能继续打印。 */
static void cycle_loop_process_uart(Cycle_Loop_Uart_Context_t *context,
                                    rt_tick_t now)
{
    /* 根据当前状态只推进状态机的一步，避免单个串口长时间占用线程。 */
    switch(context->state) {
    case CYCLE_LOOP_SCAN_READY:
        /* 固定探测阶段发送地址探测请求。 */
        if(context->phase == CYCLE_LOOP_PHASE_PROBE) {
            cycle_loop_send_probe_request(context);
        }
        /* 特征识别阶段发送当前有效协议的特征寄存器请求。 */
        else {
            cycle_loop_send_feature_request(context);
        }
        break;

    case CYCLE_LOOP_SCAN_WAIT_RESPONSE:
        /* 已收到完整报文时立即解析，不再计算本次请求是否超时。 */
        if(context->rx_ready == RT_TRUE) {
            /* 收到任意完整帧即结束本次超时等待，解析结果负责决定下一地址或下一协议。 */
            cycle_loop_handle_response(context);
        }
        /* 使用无符号tick差值可兼容rt_tick_get()计数自然回绕。 */
        else if((rt_tick_t)(now - context->request_tick) >=
                CYCLE_LOOP_RESPONSE_TIMEOUT_TICKS) {
            /* 固定探测超时后增加Modbus地址，地址10超时后停止端口。 */
            if(context->phase == CYCLE_LOOP_PHASE_PROBE) {
                rt_kprintf("%s uart[%d] addr[%d] no address probe reply within 1s\n", get_char_time(), context->uart_no, context->slave_addr);
                cycle_loop_advance_probe_addr(context);
            }
            /* 特征识别超时后保留设备地址，只切换到下一条有效协议。 */
            else {
                rt_kprintf("%s uart[%d] protocol[%d] no feature reply within 1s\n", get_char_time(), context->uart_no, context->protocol_index);
                cycle_loop_advance_protocol(context);
            }
        }
        break;

    case CYCLE_LOOP_SCAN_STOPPED:
    default:
        break;
    }
}

/* 只启动空闲端口的识别状态机，已有有效档案占用的端口保持STOPPED。 */
static uint8_t Inv_Archive_Idle_Uarts(void)
{
    uint8_t index;
    uint8_t scan_count = 0U;

    rt_memset(g_scan_uarts, 0, sizeof(g_scan_uarts));

    /* 三个物理串口分别判断对应档案端口是否空闲。 */
    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index) {
        /* 扫描上下文只保存串口号，写档案时再通过映射函数查询档案端口。 */
        g_scan_uarts[index].uart_no = g_scan_port_map[index].uart_no;
        g_scan_uarts[index].phase = CYCLE_LOOP_PHASE_PROBE;
        g_scan_uarts[index].slave_addr = CYCLE_LOOP_SCAN_ADDR_FIRST;

        /* 端口已有有效档案时跳过自动识别，避免干扰已建档设备。 */
        if(Inv_Archive_Port_Is_Occupied(g_scan_port_map[index].archive_port) != 0U) {
            g_scan_uarts[index].state = CYCLE_LOOP_SCAN_STOPPED;
            rt_kprintf("%s uart[%d] port[%d] already has an archive, device scan skipped\n", get_char_time(), g_scan_port_map[index].uart_no, g_scan_port_map[index].archive_port);
        }
        /* 端口没有有效档案时启动独立的自动识别状态机。 */
        else {
            g_scan_uarts[index].state = CYCLE_LOOP_SCAN_READY;
            ++scan_count;
            rt_kprintf("%s uart[%d] port[%d] is free, device scan started\n", get_char_time(), g_scan_port_map[index].uart_no, g_scan_port_map[index].archive_port);
        }
    }

    rt_kprintf("%s scan all inv archive ports, found %d free ports\n", get_char_time(), scan_count);

    return scan_count;
}

/* 所有上下文均停止时，空闲端口识别流程已经全部结束。 */
static rt_bool_t cycle_loop_all_scan_uarts_stopped(void)
{
    uint8_t index;

    /* 只要任意一个串口没有停止，自动识别流程就仍需继续运行。 */
    for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index) {
        /* 当前串口仍在发送或等待响应时返回未全部停止。 */
        if(g_scan_uarts[index].state != CYCLE_LOOP_SCAN_STOPPED) {
            return RT_FALSE;
        }
    }

    return RT_TRUE;
}

rt_err_t cycle_loop_rx_frame(uint16_t uart_no,
                             const uint8_t *frame,
                             uint16_t frame_len)
{
    Cycle_Loop_Uart_Context_t *context;
    rt_base_t level;

    /* 报文指针为空、长度为0或超过接收邮箱容量时拒绝接收。 */
    if((frame == RT_NULL) || (frame_len == 0U) ||
       (frame_len > CYCLE_LOOP_RX_FRAME_SIZE)) {
        return -RT_EINVAL;
    }

    context = cycle_loop_find_uart(uart_no);

    /* 非扫描串口或当前没有等待响应时，不允许向状态机提交报文。 */
    if((context == RT_NULL) ||
       (context->state != CYCLE_LOOP_SCAN_WAIT_RESPONSE)) {
        return -RT_EBUSY;               /* 非扫描端口或非等待状态不接收响应。 */
    }

    level = rt_hw_interrupt_disable();

    /* 上一帧尚未处理时禁止覆盖单帧邮箱。 */
    if(context->rx_ready == RT_TRUE) {
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

    /* 无论存档count为何值都重新校验有效槽位，防止协议库变化后档案仍被误用。 */
    Inv_Archive_Validate_Protocols();
    // 扫描空闲串口
    Inv_Archive_Idle_Uarts();

    /* 任意串口仍在识别时持续轮询三个独立状态机。 */
    // 轮询串口，处于空闲的会进行串口识别
    while(cycle_loop_all_scan_uarts_stopped() == RT_FALSE) {
        rt_tick_t now = rt_tick_get();

        /* 顺序推进三个独立状态机，不会等待前一个串口超时后才处理下一个串口。 */
        for(index = 0U; index < CYCLE_LOOP_SCAN_PORT_COUNT; ++index) {
            cycle_loop_process_uart(&g_scan_uarts[index], now);
        }

        /* 仍有端口运行时延时10ms，避免轮询线程持续占满CPU。 */
        if(cycle_loop_all_scan_uarts_stopped() == RT_FALSE) {
            rt_thread_mdelay(CYCLE_LOOP_THREAD_POLL_MS);
        }
    }

    /* 没有有效档案时不进入周期抄读。 */
    if(g_inv_archive_lib.count == 0U) {
        rt_kprintf("%s no valid archive, periodic reading will not start\n", get_char_time());
    }
    /* 存在有效档案时进入周期抄读，后续由该循环持续推进三个端口状态机。 */
    else {
        rt_kprintf("%s periodic reading started for [%d] archives\n", get_char_time(), g_inv_archive_lib.count);
        Inv_Data_Poll_Loop();
    }
}
