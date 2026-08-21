/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-16     mutou       the first version
 */
#include "inv_data.h"

#include <limits.h>
#include <rthw.h>

#include "inverter_protocol_library.h"
#include "main_uart.h"
#include "modbus_master.h"
#include "sys.h"
#include "uart_def.h"

#define INV_DATA_PORT_COUNT                3U
#define INV_DATA_POINT_COUNT              31U
#define INV_DATA_RESPONSE_TIMEOUT_TICKS  1000U
#define INV_DATA_DEVICE_IDLE_TICKS      10000U
#define INV_DATA_POLL_TICKS               10U
#define INV_DATA_RX_FRAME_SIZE            MODBUS_RTU_ADU_MAX
#define INV_DATA_NUMERIC_BYTE_MAX          8U
#define INV_DATA_PHASE_COMBINE_MAX         3U
#define INV_CONTROL_QUEUE_SIZE             4U
#define INV_CONTROL_RESULT_QUEUE_SIZE     16U
#define INV_CONTROL_REGISTER_MAX          (INV_DATA_NUMERIC_BYTE_MAX / 2U)

/* 每个端口的抄读状态独立变化，等待某一路响应时其他端口仍可继续发送或解析。 */
typedef enum Inv_Data_Port_State {
    INV_DATA_PORT_READY = 0,          /* 当前端口可以查找并发送下一项寄存器请求。 */
    INV_DATA_PORT_WAIT_RESPONSE,      /* 当前端口已经发送请求，正在等待响应或超时。 */
    INV_DATA_PORT_DEVICE_IDLE         /* 当前端口完成一台逆变器抄读，正在空闲10秒。 */
} Inv_Data_Port_State_t;

/* 同一接收邮箱同时服务周期读取和实时控制，当前请求类型决定响应解析方式。 */
typedef enum Inv_Data_Request_Type {
    INV_DATA_REQUEST_NONE = 0,       /* 当前端口没有已经发送并等待响应的请求。 */
    INV_DATA_REQUEST_PERIODIC_READ,  /* 当前端口正在等待周期读取响应。 */
    INV_DATA_REQUEST_CONTROL_WRITE   /* 当前端口正在等待实时控制写响应。 */
} Inv_Data_Request_Type_t;

/* 当前待抄读点的统一描述，数据类、参数类和控制类最终都转换为该格式。 */
typedef struct Inv_Data_Point_Config {
    uint16_t reg_addr;                       /* Modbus请求中直接使用的寄存器起始地址。 */
    uint16_t reg_count;                      /* 本次请求连续读取的16位寄存器数量。 */
    uint8_t function_code;                   /* 读取寄存器使用的Modbus功能码0x03或0x04。 */
    uint8_t data_type;                       /* 寄存器数据解析类型，使用TYPE_*定义。 */
    uint8_t byte_order;                      /* 多字节数据排列方式，使用Type_Byte_t定义。 */
    uint8_t decimal_places;                  /* 实时值采用定点整数保存时保留的小数位数。 */
    Inv_RealtimeValue_t *number_target;      /* 数值型数据解析成功后的实时数据存储地址。 */
    Inv_RealtimeString_t *string_target;     /* 设备编号等字符串数据解析成功后的存储地址。 */
    const char *name;                        /* 当前数据点用于日志打印的可读名称。 */
} Inv_Data_Point_Config_t;

/* 当前控制命令转换后的寄存器配置和写入数据全部保存在端口上下文中。 */
typedef struct Inv_Control_Active {
    Inv_Control_Request_t request;                  /* 当前正在执行的控制请求。 */
    uint16_t reg_addr;                              /* 控制寄存器起始地址。 */
    uint16_t reg_count;                             /* 本次控制连续写入的寄存器数量。 */
    uint16_t registers[INV_CONTROL_REGISTER_MAX];  /* 已按协议字节序转换的Modbus寄存器数据。 */
    uint8_t function_code;                          /* 控制寄存器使用的06或10功能码。 */
    uint8_t data_type;                              /* 控制值的数据类型，使用TYPE_*定义。 */
    uint8_t byte_order;                             /* 控制值多字节排列方式。 */
    uint8_t decimal_places;                         /* 浮点控制值转换时使用的小数位数。 */
    Inv_RealtimeValue_t *target;                    /* 控制成功后需要更新的实时控制数据。 */
    const char *name;                               /* 当前控制项用于日志打印的名称。 */
} Inv_Control_Active_t;

/* 每个物理端口使用独立的控制请求环形队列，端口忙时不会阻塞提交线程。 */
typedef struct Inv_Control_Queue {
    Inv_Control_Request_t requests[INV_CONTROL_QUEUE_SIZE]; /* 等待执行的控制请求。 */
    uint8_t read_index;                                      /* 下一项待取控制请求下标。 */
    uint8_t write_index;                                     /* 下一项待写控制请求下标。 */
    uint8_t count;                                           /* 当前队列中的有效请求数量。 */
} Inv_Control_Queue_t;

/* 单个端口上下文保存独立档案游标、当前请求、超时tick及接收邮箱。 */
typedef struct Inv_Data_Port_Context {
    uint16_t uart_no;                        /* 串口管理模块使用的逻辑串口编号。 */
    uint8_t archive_port;                    /* 当前串口对应的逆变器档案接入端口。 */
    uint8_t archive_index;                   /* 下一次查找数据点使用的档案槽位游标。 */
    uint8_t point_index;                     /* 下一次查找使用的数据点下标，范围0～30。 */
    uint8_t active_archive_index;            /* 当前已发送请求所属的档案槽位下标。 */
    uint8_t slave_addr;                      /* 当前已发送请求使用的Modbus从站地址。 */
    Inv_Data_Port_State_t state;             /* 当前端口处于可发送、等待响应或空闲状态。 */
    Inv_Data_Port_State_t resume_state;      /* 实时控制完成后需要恢复的周期抄读状态。 */
    Inv_Data_Request_Type_t request_type;    /* 当前等待响应的是周期读取还是实时控制。 */
    Inv_Data_Point_Config_t active_point;    /* 当前已发送请求中第一个数据点的完整配置。 */
    Inv_Data_Point_Config_t combined_points[INV_DATA_PHASE_COMBINE_MAX - 1U]; /* 连续读取时追加的三相电压或三相电流数据点。 */
    uint8_t active_point_index;              /* 当前请求中第一个数据点在协议数据点表中的下标。 */
    uint8_t active_point_count;              /* 当前请求合并的数据点数量，普通请求固定为1。 */
    uint16_t active_reg_count;               /* 当前请求包含的寄存器总数，用于组帧及校验响应长度。 */
    Inv_Control_Active_t active_control;     /* 当前已经发送并等待响应的控制命令。 */
    Inv_Control_Queue_t control_queue;       /* 当前端口等待发送的高优先级控制请求。 */
    rt_tick_t request_tick;                  /* 当前请求完整写入串口时记录的系统tick。 */
    rt_tick_t idle_tick;                     /* 当前逆变器完成全部抄读并进入空闲时的系统tick。 */
    rt_bool_t idle_after_active;             /* RT_TRUE表示当前数据点完成后需要进入10秒空闲。 */
    uint8_t rx_frame[INV_DATA_RX_FRAME_SIZE]; /* 当前端口接收到的完整Modbus响应报文。 */
    uint16_t rx_frame_len;                   /* rx_frame缓冲区中的有效响应报文长度。 */
    volatile rt_bool_t rx_ready;             /* RT_TRUE表示接收邮箱中存在待解析响应。 */
} Inv_Data_Port_Context_t;

/* 实时数据数组下标与档案槽位下标固定对应，数据不写入Flash。 */
Inv_Data_t g_inv_data[INVERTER_ARCHIVE_MAX_COUNT];

/* 三个有线端口分别维护一套完全独立的周期抄读状态。 */
static Inv_Data_Port_Context_t g_inv_data_ports[INV_DATA_PORT_COUNT];

/* 控制结果使用独立环形队列，后续645模块可以通过查询接口异步取走。 */
static Inv_Control_Result_Info_t g_inv_control_results[INV_CONTROL_RESULT_QUEUE_SIZE];
static uint8_t g_inv_control_result_read_index;  /* 下一项待取控制结果下标。 */
static uint8_t g_inv_control_result_write_index; /* 下一项待写控制结果下标。 */
static uint8_t g_inv_control_result_count;       /* 当前结果队列中的有效结果数量。 */
static volatile rt_bool_t g_inv_data_initialized; /* RT_TRUE表示端口上下文已经完成初始化。 */

/* 数据类各数组对应的日志名称。 */
static const char *g_inv_data_u_name[ENUM_PHASE_MAX] = {"Ua", "Ub", "Uc"};
static const char *g_inv_data_i_name[ENUM_PHASE_MAX] = {"Ia", "Ib", "Ic"};
static const char *g_inv_data_p_name[ENUM_PMAX] = {"Pa", "Pb", "Pc", "Pt"};
static const char *g_inv_data_q_name[ENUM_QMAX] = {"Qa", "Qb", "Qc", "Qt"};
static const char *g_inv_data_pf_name[ENUM_PFMAX] = {"PFa", "PFb", "PFc", "PFt"};

/* 按档案接入端口查找对应的周期抄读端口上下文。 */
static Inv_Data_Port_Context_t *inv_control_find_port_context(uint8_t archive_index)
{
    uint8_t archive_port;
    uint8_t index;

    /* 档案下标越界或档案当前无效时不能查找下行端口。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID)) {
        return RT_NULL;
    }

    archive_port = g_inv_archive_lib.archives[archive_index].port;

    /* 三个端口上下文的archive_port与档案中的接入端口使用同一套编号。 */
    for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
        if(g_inv_data_ports[index].archive_port == archive_port) {
            return &g_inv_data_ports[index];
        }
    }

    return RT_NULL;
}

/* 从端口控制队列安全取出最早提交的一项请求。 */
static rt_bool_t inv_control_queue_get(Inv_Control_Queue_t *queue, Inv_Control_Request_t *request)
{
    rt_base_t level;

    /* 队列或输出指针为空时不能读取控制请求。 */
    if((queue == RT_NULL) || (request == RT_NULL)) {
        return RT_FALSE;
    }

    level = rt_hw_interrupt_disable();

    /* 队列为空时恢复中断并返回没有请求。 */
    if(queue->count == 0U) {
        rt_hw_interrupt_enable(level);
        return RT_FALSE;
    }

    *request = queue->requests[queue->read_index];
    queue->read_index = (queue->read_index + 1U) % INV_CONTROL_QUEUE_SIZE;
    --queue->count;
    rt_hw_interrupt_enable(level);
    return RT_TRUE;
}

/* 查询端口控制队列是否存在待执行命令，读取计数时使用临界区避免并发提交。 */
static rt_bool_t inv_control_queue_has_request(Inv_Control_Queue_t *queue)
{
    rt_base_t level;
    rt_bool_t has_request;

    level = rt_hw_interrupt_disable();
    has_request = (queue->count > 0U) ? RT_TRUE : RT_FALSE;
    rt_hw_interrupt_enable(level);
    return has_request;
}

/* 将控制最终结果写入公共结果队列，队列满时保留最新结果并丢弃最旧结果。 */
static void inv_control_push_result(const Inv_Control_Request_t *request,
                                    Inv_Control_Result_t result,
                                    uint8_t exception_code)
{
    Inv_Control_Result_Info_t result_info;
    rt_base_t level;

    rt_memset(&result_info, 0, sizeof(result_info));
    result_info.request = *request;
    result_info.result = result;
    result_info.exception_code = exception_code;
    result_info.finish_tick = rt_tick_get();

    level = rt_hw_interrupt_disable();

    /* 结果长期未读取导致队列已满时，移动读下标以保留本次最新控制结果。 */
    if(g_inv_control_result_count >= INV_CONTROL_RESULT_QUEUE_SIZE) {
        g_inv_control_result_read_index = (g_inv_control_result_read_index + 1U) % INV_CONTROL_RESULT_QUEUE_SIZE;
        --g_inv_control_result_count;
    }

    g_inv_control_results[g_inv_control_result_write_index] = result_info;
    g_inv_control_result_write_index = (g_inv_control_result_write_index + 1U) % INV_CONTROL_RESULT_QUEUE_SIZE;
    ++g_inv_control_result_count;
    rt_hw_interrupt_enable(level);
}

/* 将只读协议寄存器复制为统一抄读配置，避免直接读取1字节对齐结构中的16位字段。 */
static void inv_data_copy_read_config(Inv_Data_Point_Config_t *point, const Inv_RegBlk_t *reg)
{
    Inv_RegBlk_t local_reg;

    rt_memcpy(&local_reg, reg, sizeof(local_reg));
    point->reg_addr = local_reg.reg_addr;
    point->reg_count = local_reg.reg_cnt;
    point->function_code = local_reg.read_func_code;
    point->data_type = local_reg.data_type;
    point->byte_order = local_reg.byte_order;
    point->decimal_places = local_reg.decimal_places;
}

/* 将普通控制寄存器转换为03功能码读保持寄存器配置。 */
static void inv_data_copy_ctrl_config(Inv_Data_Point_Config_t *point, const Inv_CtrlRegBlk_t *reg)
{
    Inv_CtrlRegBlk_t local_reg;

    rt_memcpy(&local_reg, reg, sizeof(local_reg));
    point->reg_addr = local_reg.reg_addr;
    point->reg_count = local_reg.reg_cnt;
    point->function_code = MODBUS_FUNC_READ_HOLDING;
    point->data_type = local_reg.data_type;
    point->byte_order = local_reg.byte_order;
    point->decimal_places = local_reg.decimal_places;
}

/* 将带默认值的开关机控制寄存器转换为03功能码读保持寄存器配置。 */
static void inv_data_copy_default_ctrl_config(Inv_Data_Point_Config_t *point, const Inv_CtrlDefaultRegBlk_t *reg)
{
    Inv_CtrlDefaultRegBlk_t local_reg;

    rt_memcpy(&local_reg, reg, sizeof(local_reg));
    point->reg_addr = local_reg.reg_addr;
    point->reg_count = local_reg.reg_cnt;
    point->function_code = MODBUS_FUNC_READ_HOLDING;
    point->data_type = local_reg.data_type;
    point->byte_order = local_reg.byte_order;
    point->decimal_places = local_reg.decimal_places;
}

/* 根据控制类型从档案协议中取得写寄存器配置和实时数据存储位置。 */
static Inv_Control_Result_t inv_control_get_active(const Inv_Control_Request_t *request,
                                                   Inv_Control_Active_t *active)
{
    const Inv_Proto_t *protocol;
    Inv_CtrlRegBlk_t ctrl_reg;
    Inv_CtrlDefaultRegBlk_t default_reg;

    /* 请求指针、输出指针、档案下标或控制类型无效时不能取得配置。 */
    if((request == RT_NULL) || (active == RT_NULL) ||
       (request->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (request->type < INV_CONTROL_POWER_ON) || (request->type >= INV_CONTROL_TYPE_MAX)) {
        return INV_CONTROL_RESULT_ARCHIVE_INVALID;
    }

    /* 控制命令排队后档案可能被其他流程置为无效，因此执行前需要重新检查。 */
    if(g_inv_archive_lib.valid[request->archive_index] != INVERTER_ARCHIVE_VALID) {
        return INV_CONTROL_RESULT_ARCHIVE_INVALID;
    }

    protocol = Inv_Archive_Get_Protocol(request->archive_index);

    /* 有效档案没有匹配协议指针时不能确定控制寄存器。 */
    if(protocol == RT_NULL) {
        return INV_CONTROL_RESULT_PROTOCOL_MISSING;
    }

    rt_memset(active, 0, sizeof(*active));
    active->request = *request;

    /* 开关机控制使用协议库中各自配置的固定默认写入值。 */
    if((request->type == INV_CONTROL_POWER_ON) || (request->type == INV_CONTROL_POWER_OFF)) {
        if(request->type == INV_CONTROL_POWER_ON) {
            rt_memcpy(&default_reg, &protocol->ctrl.pwr_on, sizeof(default_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.pwr_on;
            active->name = "power_on";
        }
        else {
            rt_memcpy(&default_reg, &protocol->ctrl.pwr_off, sizeof(default_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.pwr_off;
            active->name = "power_off";
        }

        active->reg_addr = default_reg.reg_addr;
        active->reg_count = default_reg.reg_cnt;
        active->function_code = default_reg.write_func_code;
        active->data_type = default_reg.data_type;
        active->byte_order = default_reg.byte_order;
        active->decimal_places = default_reg.decimal_places;
        active->request.value = default_reg.write_default_val;
    }
    /* 其他控制项使用调用方提供的定点整数值。 */
    else {
        switch(request->type) {
        case INV_CONTROL_ACTIVE_POWER:
            rt_memcpy(&ctrl_reg, &protocol->ctrl.active_pwr_ctrl, sizeof(ctrl_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.active_pwr_ctrl;
            active->name = "active_power";
            break;

        case INV_CONTROL_REACTIVE_POWER:
            rt_memcpy(&ctrl_reg, &protocol->ctrl.reactive_pwr_ctrl, sizeof(ctrl_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.reactive_pwr_ctrl;
            active->name = "reactive_power";
            break;

        case INV_CONTROL_POWER_FACTOR:
            rt_memcpy(&ctrl_reg, &protocol->ctrl.pwr_factor_ctrl, sizeof(ctrl_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.pwr_factor_ctrl;
            active->name = "power_factor";
            break;

        case INV_CONTROL_ACTIVE_POWER_PERCENT:
            rt_memcpy(&ctrl_reg, &protocol->ctrl.active_pwr_pct_ctrl, sizeof(ctrl_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.active_pwr_pct_ctrl;
            active->name = "active_power_percent";
            break;

        default:
            rt_memcpy(&ctrl_reg, &protocol->ctrl.reactive_pwr_pct_ctrl, sizeof(ctrl_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.reactive_pwr_pct_ctrl;
            active->name = "reactive_power_percent";
            break;
        }

        active->reg_addr = ctrl_reg.reg_addr;
        active->reg_count = ctrl_reg.reg_cnt;
        active->function_code = ctrl_reg.write_func_code;
        active->data_type = ctrl_reg.data_type;
        active->byte_order = ctrl_reg.byte_order;
        active->decimal_places = ctrl_reg.decimal_places;
    }

    /* 未配置地址、寄存器数量超出内部编码容量或写功能码不匹配时不支持控制。 */
    if((active->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) ||
       (active->reg_count == 0U) || (active->reg_count > INV_CONTROL_REGISTER_MAX) ||
       ((active->function_code != MODBUS_FUNC_WRITE_SINGLE) &&
        (active->function_code != MODBUS_FUNC_WRITE_MULTIPLE))) {
        return INV_CONTROL_RESULT_UNSUPPORTED;
    }

    /* 06功能码只能写一个寄存器，10功能码数量还必须符合Modbus上限。 */
    if(((active->function_code == MODBUS_FUNC_WRITE_SINGLE) && (active->reg_count != 1U)) ||
       ((active->function_code == MODBUS_FUNC_WRITE_MULTIPLE) &&
        (active->reg_count > MODBUS_WRITE_REG_MAX))) {
        return INV_CONTROL_RESULT_UNSUPPORTED;
    }

    return INV_CONTROL_RESULT_OK;
}

/* 按0～30数据点下标取得协议寄存器配置和对应实时数据存储位置。 */
static rt_bool_t inv_data_get_point(uint8_t archive_index,
                                    uint8_t point_index,
                                    Inv_Data_Point_Config_t *point)
{
    const Inv_Proto_t *protocol;
    Inv_Data_t *data;
    uint8_t offset;

    /* 档案下标、数据点下标或输出指针无效时无法取得抄读点。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (point_index >= INV_DATA_POINT_COUNT) || (point == RT_NULL)) {
        return RT_FALSE;
    }

    protocol = Inv_Archive_Get_Protocol(archive_index);

    /* 档案没有匹配协议时不能取得寄存器配置。 */
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    data = &g_inv_data[archive_index];
    rt_memset(point, 0, sizeof(*point));

    /* 下标0～2对应A、B、C三相电压。 */
    if(point_index < ENUM_PHASE_MAX) {
        offset = point_index;
        inv_data_copy_read_config(point, &protocol->data.Ux[offset]);
        point->number_target = &data->data.Ux[offset];
        point->name = g_inv_data_u_name[offset];
    }
    /* 下标3～5对应A、B、C三相电流。 */
    else if(point_index < (ENUM_PHASE_MAX * 2U)) {
        offset = point_index - ENUM_PHASE_MAX;
        inv_data_copy_read_config(point, &protocol->data.Ix[offset]);
        point->number_target = &data->data.Ix[offset];
        point->name = g_inv_data_i_name[offset];
    }
    /* 下标6～9对应A、B、C三相及总有功功率。 */
    else if(point_index < (ENUM_PHASE_MAX * 2U + ENUM_PMAX)) {
        offset = point_index - ENUM_PHASE_MAX * 2U;
        inv_data_copy_read_config(point, &protocol->data.Px[offset]);
        point->number_target = &data->data.Px[offset];
        point->name = g_inv_data_p_name[offset];
    }
    /* 下标10～13对应A、B、C三相及总无功功率。 */
    else if(point_index < (ENUM_PHASE_MAX * 2U + ENUM_PMAX + ENUM_QMAX)) {
        offset = point_index - ENUM_PHASE_MAX * 2U - ENUM_PMAX;
        inv_data_copy_read_config(point, &protocol->data.Qx[offset]);
        point->number_target = &data->data.Qx[offset];
        point->name = g_inv_data_q_name[offset];
    }
    /* 下标14～17对应A、B、C三相及总功率因数。 */
    else if(point_index < 18U) {
        offset = point_index - 14U;
        inv_data_copy_read_config(point, &protocol->data.PFx[offset]);
        point->number_target = &data->data.PFx[offset];
        point->name = g_inv_data_pf_name[offset];
    }
    /* 下标18～23对应参数类的6个只读数据点。 */
    else if(point_index < 24U) {
        switch(point_index) {
        case 18U:
            inv_data_copy_read_config(point, &protocol->param.dev_no);
            point->string_target = &data->param.dev_no;
            point->name = "device_no";
            break;

        case 19U:
            inv_data_copy_read_config(point, &protocol->param.pv_rated_active_pwr);
            point->number_target = &data->param.pv_rated_active_pwr;
            point->name = "pv_rated_p";
            break;

        case 20U:
            inv_data_copy_read_config(point, &protocol->param.pv_rated_reactive_pwr);
            point->number_target = &data->param.pv_rated_reactive_pwr;
            point->name = "pv_rated_q";
            break;

        case 21U:
            inv_data_copy_read_config(point, &protocol->param.set_volt);
            point->number_target = &data->param.set_volt;
            point->name = "set_voltage";
            break;

        case 22U:
            inv_data_copy_read_config(point, &protocol->param.output_type);
            point->number_target = &data->param.output_type;
            point->name = "output_type";
            break;

        default:
            inv_data_copy_read_config(point, &protocol->param.pwr_status);
            point->number_target = &data->param.pwr_status;
            point->name = "power_status";
            break;
        }
    }
    /* 下标24～30对应控制类的7个保持寄存器数据点。 */
    else {
        switch(point_index) {
        case 24U:
            inv_data_copy_default_ctrl_config(point, &protocol->ctrl.pwr_on);
            point->number_target = &data->ctrl.pwr_on;
            point->name = "power_on";
            break;

        case 25U:
            inv_data_copy_default_ctrl_config(point, &protocol->ctrl.pwr_off);
            point->number_target = &data->ctrl.pwr_off;
            point->name = "power_off";
            break;

        case 26U:
            inv_data_copy_ctrl_config(point, &protocol->ctrl.active_pwr_ctrl);
            point->number_target = &data->ctrl.active_pwr_ctrl;
            point->name = "active_pwr_ctrl";
            break;

        case 27U:
            inv_data_copy_ctrl_config(point, &protocol->ctrl.reactive_pwr_ctrl);
            point->number_target = &data->ctrl.reactive_pwr_ctrl;
            point->name = "reactive_pwr_ctrl";
            break;

        case 28U:
            inv_data_copy_ctrl_config(point, &protocol->ctrl.pwr_factor_ctrl);
            point->number_target = &data->ctrl.pwr_factor_ctrl;
            point->name = "power_factor_ctrl";
            break;

        case 29U:
            inv_data_copy_ctrl_config(point, &protocol->ctrl.active_pwr_pct_ctrl);
            point->number_target = &data->ctrl.active_pwr_pct_ctrl;
            point->name = "active_pwr_pct_ctrl";
            break;

        default:
            inv_data_copy_ctrl_config(point, &protocol->ctrl.reactive_pwr_pct_ctrl);
            point->number_target = &data->ctrl.reactive_pwr_pct_ctrl;
            point->name = "reactive_pwr_pct_ctrl";
            break;
        }
    }

    return RT_TRUE;
}

/* 判断数据点是否具有可以组成Modbus读请求的完整寄存器配置。 */
static rt_bool_t inv_data_point_is_readable(const Inv_Data_Point_Config_t *point)
{
    /* 地址0xFFFF或寄存器数量0表示厂家协议没有配置该数据点。 */
    if((point->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) || (point->reg_count == 0U)) {
        return RT_FALSE;
    }

    /* 单次读取数量超过Modbus最大值时不能组成合法请求。 */
    if(point->reg_count > MODBUS_READ_REG_MAX) {
        return RT_FALSE;
    }

    /* 数据类和参数类只允许03或04功能码，控制类已经统一转换为03功能码。 */
    if((point->function_code != MODBUS_FUNC_READ_HOLDING) &&
       (point->function_code != MODBUS_FUNC_READ_INPUT)) {
        return RT_FALSE;
    }

    return RT_TRUE;
}

/* 当前数据点完成后推进游标，31个点结束后切换到下一个档案槽位。 */
static void inv_data_advance_cursor(Inv_Data_Port_Context_t *context)
{
    ++context->point_index;

    /* 当前档案的31个数据点已经遍历完成时切换下一档案。 */
    if(context->point_index >= INV_DATA_POINT_COUNT) {
        context->point_index = 0U;
        ++context->archive_index;

        /* 档案槽位11完成后回到槽位0，开始下一轮周期抄读。 */
        if(context->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) {
            context->archive_index = 0U;
        }
    }
}

/* 当前端口完成一台逆变器后进入独立的10秒空闲状态。 */
static void inv_data_start_device_idle(Inv_Data_Port_Context_t *context, uint8_t archive_index)
{
    context->idle_tick = rt_tick_get();
    context->idle_after_active = RT_FALSE;
    context->state = INV_DATA_PORT_DEVICE_IDLE;
    rt_kprintf("[%08d] uart[%d] archive[%d] all data read, wait 10s before next device\n", rt_tick_get(), context->uart_no, archive_index + 1);
}

/* 从当前游标开始查找本端口下一项有效且已配置的寄存器。 */
static rt_bool_t inv_data_find_next_point(Inv_Data_Port_Context_t *context)
{
    uint16_t attempt;

    /* 最多检查12个档案的全部31个点，避免没有可读点时形成死循环。 */
    for(attempt = 0U; attempt < (INVERTER_ARCHIVE_MAX_COUNT * INV_DATA_POINT_COUNT); ++attempt) {
        uint8_t archive_index = context->archive_index;
        uint8_t point_index = context->point_index;
        const Inv_Archive_t *archive = &g_inv_archive_lib.archives[archive_index];
        Inv_Data_Point_Config_t point;
        rt_bool_t archive_last_point = (point_index == (INV_DATA_POINT_COUNT - 1U)) ? RT_TRUE : RT_FALSE;

        inv_data_advance_cursor(context); /* 先移动下一游标，当前点完成后可以直接继续查找。 */

        /* 无效档案或接入其他端口的档案不属于当前端口状态机。 */
        if((g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) ||
           (archive->port != context->archive_port)) {
            continue;
        }

        /* 无法取得协议数据点或寄存器未配置时直接检查下一点。 */
        if((inv_data_get_point(archive_index, point_index, &point) == RT_FALSE) ||
           (inv_data_point_is_readable(&point) == RT_FALSE)) {
            /* 当前点是该逆变器最后一项时，即使未配置寄存器也需要进入10秒空闲。 */
            if(archive_last_point == RT_TRUE) {
                inv_data_start_device_idle(context, archive_index);
                return RT_FALSE;
            }

            continue;
        }

        context->active_archive_index = archive_index;
        context->slave_addr = archive->mb_addr;
        context->active_point = point;
        context->active_point_index = point_index;
        context->active_point_count = 1U;
        context->active_reg_count = point.reg_count;
        context->idle_after_active = archive_last_point;
        return RT_TRUE;
    }

    return RT_FALSE;
}

/* 按合并下标取得当前请求中的数据点配置，下标0对应第一个数据点。 */
static Inv_Data_Point_Config_t *inv_data_get_active_point(Inv_Data_Port_Context_t *context, uint8_t point_index)
{
    /* 第一个数据点继续使用原有active_point成员，减少普通数据点读取逻辑的改动。 */
    if(point_index == 0U) {
        return &context->active_point;
    }

    return &context->combined_points[point_index - 1U];
}

/* 将地址连续且功能码相同的三相电压或三相电流合并到当前Modbus读请求。 */
static void inv_data_combine_phase_points(Inv_Data_Port_Context_t *context)
{
    Inv_Data_Point_Config_t next_point;
    Inv_Data_Point_Config_t *previous_point;
    uint8_t group_last_index;
    uint8_t next_point_index;

    /* 数据点0～2是三相电压，数据点3～5是三相电流，其他类型不进行合并。 */
    if(context->active_point_index < ENUM_PHASE_MAX) {
        group_last_index = ENUM_PHASE_MAX - 1U;
    }
    else if(context->active_point_index < (ENUM_PHASE_MAX * 2U)) {
        group_last_index = ENUM_PHASE_MAX * 2U - 1U;
    }
    else {
        return;
    }

    /* 最多合并同一组中的三个数据点，遇到地址不连续或配置不同立即停止。 */
    while(context->active_point_count < INV_DATA_PHASE_COMBINE_MAX) {
        next_point_index = context->active_point_index + context->active_point_count;

        /* 当前点已经是本组三相数据最后一点时没有后续数据可以合并。 */
        if(next_point_index > group_last_index) {
            break;
        }

        /* 游标不在同一档案的紧邻下一点时不能越过其他数据点进行合并。 */
        if((context->archive_index != context->active_archive_index) ||
           (context->point_index != next_point_index)) {
            break;
        }

        /* 后一个三相数据点未配置有效寄存器时保留给正常游标逻辑处理。 */
        if((inv_data_get_point(context->active_archive_index, next_point_index, &next_point) == RT_FALSE) ||
           (inv_data_point_is_readable(&next_point) == RT_FALSE)) {
            break;
        }

        previous_point = inv_data_get_active_point(context, context->active_point_count - 1U);

        /* 只有功能码相同并且后一个起始地址紧跟前一个寄存器结束地址时才能合并。 */
        if((next_point.function_code != context->active_point.function_code) ||
           (next_point.reg_addr != (previous_point->reg_addr + previous_point->reg_count))) {
            break;
        }

        /* 合并后的寄存器总数不能超过Modbus单次读寄存器数量上限。 */
        if((context->active_reg_count + next_point.reg_count) > MODBUS_READ_REG_MAX) {
            break;
        }

        context->combined_points[context->active_point_count - 1U] = next_point;
        ++context->active_point_count;
        context->active_reg_count += next_point.reg_count;
        inv_data_advance_cursor(context); /* 后一个数据点已并入当前请求，游标同步移动到再下一点。 */
    }

    /* 实际合并成功时打印批量读取范围，普通单点读取不增加额外日志。 */
    if(context->active_point_count > 1U) {
        rt_kprintf("[%08d] uart[%d] archive[%d] combined read starts at[%s], values[%d], registers[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_point.name, context->active_point_count, context->active_reg_count);
    }
}

/* 当前请求失败或超时时清除请求内全部目标有效标志，保留旧值供故障分析。 */
static void inv_data_invalidate_active_point(Inv_Data_Port_Context_t *context)
{
    uint8_t point_index;

    /* 合并请求中的任意一点不能独立确认有效，因此失败时统一清除有效标志。 */
    for(point_index = 0U; point_index < context->active_point_count; ++point_index) {
        Inv_Data_Point_Config_t *point = inv_data_get_active_point(context, point_index);

        /* 数值型目标存在时清除数值有效标志。 */
        if(point->number_target != RT_NULL) {
            point->number_target->valid = 0U;
        }

        /* 字符串目标存在时清除字符串有效标志。 */
        if(point->string_target != RT_NULL) {
            point->string_target->valid = 0U;
        }
    }
}

/* 当前已发送数据点结束后，根据档案边界进入下一数据点或10秒空闲。 */
static void inv_data_finish_active_point(Inv_Data_Port_Context_t *context)
{
    context->request_type = INV_DATA_REQUEST_NONE; /* 当前周期读事务到此结束，端口可以接受控制写请求。 */

    /* 当前点是该逆变器最后一项时，完成后进入该端口独立的10秒空闲。 */
    if(context->idle_after_active == RT_TRUE) {
        inv_data_start_device_idle(context, context->active_archive_index);
    }
    /* 当前逆变器仍有后续数据点时直接恢复READY状态。 */
    else {
        context->state = INV_DATA_PORT_READY;
    }
}

/* 按协议字节序将Modbus寄存器转换为连续字节，返回实际写入字节数。 */
static uint16_t inv_data_registers_to_bytes(const uint16_t *registers,
                                            uint16_t register_count,
                                            uint8_t byte_order,
                                            uint8_t *output,
                                            uint16_t output_size)
{
    uint8_t source[MODBUS_RTU_ADU_MAX];
    uint16_t byte_count = register_count * 2U;
    uint16_t index;

    /* 输入指针为空或输出容量为0时不能转换。 */
    if((registers == RT_NULL) || (output == RT_NULL) || (output_size == 0U)) {
        return 0U;
    }

    /* 输出缓冲区小于寄存器数据时，只转换调用方能够保存的前部数据。 */
    if(byte_count > output_size) {
        byte_count = output_size;
    }

    /* 先恢复Modbus线上高字节在前的原始字节序列。 */
    for(index = 0U; index < byte_count; ++index) {
        uint16_t register_index = index / 2U;

        /* 偶数下标取寄存器高字节，奇数下标取寄存器低字节。 */
        if((index & 1U) == 0U) {
            source[index] = (uint8_t)(registers[register_index] >> 8U);
        }
        else {
            source[index] = (uint8_t)registers[register_index];
        }
    }

    /* BADC表示每个16位寄存器内部交换两个字节。 */
    if(byte_order == Type_Byte_BADC) {
        /* 每次处理一个16位寄存器对应的两个字节。 */
        for(index = 0U; index + 1U < byte_count; index += 2U) {
            output[index] = source[index + 1U];
            output[index + 1U] = source[index];
        }
    }
    /* DCBA表示整个数据的全部字节完全反序。 */
    else if(byte_order == Type_Byte_DCBA) {
        /* 从源数据末尾开始逐字节复制到输出缓冲区。 */
        for(index = 0U; index < byte_count; ++index) {
            output[index] = source[byte_count - 1U - index];
        }
    }
    /* CDAB表示交换16位寄存器顺序，单寄存器时交换寄存器内部字节。 */
    else if(byte_order == Type_Byte_CDAB) {
        /* 只有一个寄存器时，CDAB表示交换寄存器内部高低字节。 */
        if(byte_count == 2U) {
            output[0] = source[1];
            output[1] = source[0];
        }
        else {
            /* 多个寄存器时，CDAB保持寄存器内部字节不变并反转寄存器顺序。 */
            for(index = 0U; index + 1U < byte_count; index += 2U) {
                uint16_t source_index = byte_count - 2U - index;
                output[index] = source[source_index];
                output[index + 1U] = source[source_index + 1U];
            }
        }
    }
    /* NORMAL及未知字节序都保持Modbus线上字节顺序。 */
    else {
        rt_memcpy(output, source, byte_count);
    }

    return byte_count;
}

/* 计算10的decimal_places次方，供浮点数据转换为int32_t定点值。 */
static int32_t inv_data_decimal_scale(uint8_t decimal_places)
{
    int32_t scale = 1;
    uint8_t index;

    /* int32_t最多安全保存10的9次方，超过9位时按9位处理。 */
    if(decimal_places > 9U) {
        decimal_places = 9U;
    }

    /* 逐次乘10得到定点数缩放倍数。 */
    for(index = 0U; index < decimal_places; ++index) {
        scale *= 10;
    }

    return scale;
}

/* 将int32_t控制值按照协议数据类型和字节序转换为Modbus写寄存器。 */
static rt_bool_t inv_control_value_to_registers(Inv_Control_Active_t *active)
{
    uint8_t logical_bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0};
    uint8_t wire_bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0};
    uint64_t raw_value = 0U;
    uint64_t raw_float64;
    uint32_t raw_float32;
    float float32_value;
    double float64_value;
    uint16_t byte_count = active->reg_count * 2U;
    uint16_t index;

    /* 当前控制接口不允许写入超过内部数值缓冲区容量的寄存器数据。 */
    if((active->reg_count == 0U) || (byte_count > INV_DATA_NUMERIC_BYTE_MAX)) {
        return RT_FALSE;
    }

    /* 根据协议数据类型检查数值范围并取得需要写入的原始位模式。 */
    switch(active->data_type) {
    case TYPE_I8:
        if((active->request.value < INT8_MIN) || (active->request.value > INT8_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint8_t)(int8_t)active->request.value;
        break;

    case TYPE_U8:
        if((active->request.value < 0) || (active->request.value > UINT8_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint8_t)active->request.value;
        break;

    case TYPE_I16:
        if((active->request.value < INT16_MIN) || (active->request.value > INT16_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint16_t)(int16_t)active->request.value;
        break;

    case TYPE_U16:
        if((active->request.value < 0) || (active->request.value > UINT16_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint16_t)active->request.value;
        break;

    case TYPE_I32:
        if(byte_count < 4U) {
            return RT_FALSE;
        }
        raw_value = (uint32_t)active->request.value;
        break;

    case TYPE_U32:
    case TYPE_BIT_FIELD:
    case TYPE_BCD_TIME:
        if((active->request.value < 0) || (byte_count < 4U)) {
            return RT_FALSE;
        }
        raw_value = (uint32_t)active->request.value;
        break;

    case TYPE_FLOAT32:
        if(byte_count < 4U) {
            return RT_FALSE;
        }
        float32_value = (float)active->request.value / inv_data_decimal_scale(active->decimal_places);
        rt_memcpy(&raw_float32, &float32_value, sizeof(raw_float32));
        raw_value = raw_float32;
        break;

    case TYPE_FLOAT64:
        if(byte_count < 8U) {
            return RT_FALSE;
        }
        float64_value = (double)active->request.value / inv_data_decimal_scale(active->decimal_places);
        rt_memcpy(&raw_float64, &float64_value, sizeof(raw_float64));
        raw_value = raw_float64;
        break;

    /* ASCII和普通BCD控制值没有统一的int32_t编码规则，因此暂不支持。 */
    case TYPE_ASCII:
    case TYPE_BCD:
    default:
        return RT_FALSE;
    }

    /* 先按照最高有效字节在前生成协议逻辑字节序列，不足位数在前部补0。 */
    for(index = 0U; index < byte_count; ++index) {
        logical_bytes[byte_count - 1U - index] = (uint8_t)(raw_value >> (index * 8U));
    }

    /* BADC将每个16位寄存器内部的两个字节交换。 */
    if(active->byte_order == Type_Byte_BADC) {
        for(index = 0U; index + 1U < byte_count; index += 2U) {
            wire_bytes[index] = logical_bytes[index + 1U];
            wire_bytes[index + 1U] = logical_bytes[index];
        }
    }
    /* DCBA将全部数据字节完全反序。 */
    else if(active->byte_order == Type_Byte_DCBA) {
        for(index = 0U; index < byte_count; ++index) {
            wire_bytes[index] = logical_bytes[byte_count - 1U - index];
        }
    }
    /* CDAB对单寄存器交换字节，对多寄存器反转寄存器顺序。 */
    else if(active->byte_order == Type_Byte_CDAB) {
        if(byte_count == 2U) {
            wire_bytes[0] = logical_bytes[1];
            wire_bytes[1] = logical_bytes[0];
        }
        else {
            for(index = 0U; index + 1U < byte_count; index += 2U) {
                uint16_t source_index = byte_count - 2U - index;
                wire_bytes[index] = logical_bytes[source_index];
                wire_bytes[index + 1U] = logical_bytes[source_index + 1U];
            }
        }
    }
    /* NORMAL保持最高有效字节在前，未知字节序配置直接判定为转换失败。 */
    else if(active->byte_order == Type_Byte_ABCD) {
        rt_memcpy(wire_bytes, logical_bytes, byte_count);
    }
    else {
        return RT_FALSE;
    }

    /* 将线上字节序列每两个字节组合为一个Modbus写寄存器。 */
    for(index = 0U; index < active->reg_count; ++index) {
        active->registers[index] = ((uint16_t)wire_bytes[index * 2U] << 8U) |
                                   wire_bytes[index * 2U + 1U];
    }

    return RT_TRUE;
}

/* 将BCD字节转换为int32_t，每个半字节必须位于0～9。 */
static rt_bool_t inv_data_decode_bcd(const uint8_t *bytes, uint16_t byte_count, int32_t *value)
{
    int32_t result = 0;
    uint16_t index;

    /* 输入或输出指针为空时不能解析BCD。 */
    if((bytes == RT_NULL) || (value == RT_NULL)) {
        return RT_FALSE;
    }

    /* 每个字节按高位十进制数字、低位十进制数字依次累加。 */
    for(index = 0U; index < byte_count; ++index) {
        uint8_t high = bytes[index] >> 4U;
        uint8_t low = bytes[index] & 0x0FU;

        /* 任意半字节大于9时说明数据不是合法BCD。 */
        if((high > 9U) || (low > 9U)) {
            return RT_FALSE;
        }

        /* 继续追加两位十进制数会溢出int32_t时停止解析。 */
        if(result > (INT32_MAX - (int32_t)(high * 10U + low)) / 100) {
            return RT_FALSE;
        }

        result = result * 100 + high * 10U + low;
    }

    *value = result;
    return RT_TRUE;
}

/* 根据协议数据类型和字节序将寄存器响应转换为int32_t实时值。 */
static rt_bool_t inv_data_decode_number(const Inv_Data_Point_Config_t *point,
                                        const uint16_t *registers,
                                        int32_t *value)
{
    uint8_t bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0};
    uint64_t raw_value = 0U;
    uint32_t raw_float;
    float float32_value;
    double float64_value;
    double scaled_value;
    uint16_t byte_count;
    uint16_t index;

    byte_count = inv_data_registers_to_bytes(registers, point->reg_count, point->byte_order, bytes, sizeof(bytes));

    /* 没有得到任何数据字节时不能生成实时值。 */
    if(byte_count == 0U) {
        return RT_FALSE;
    }

    /* 连续字节按高字节在前组合为无符号原始值。 */
    for(index = 0U; index < byte_count; ++index) {
        raw_value = (raw_value << 8U) | bytes[index];
    }

    /* 不同协议数据类型最终统一转换为int32_t保存。 */
    switch(point->data_type) {
    case TYPE_I8:
        *value = (int8_t)raw_value;
        break;

    case TYPE_U8:
        *value = (uint8_t)raw_value;
        break;

    case TYPE_I16:
        *value = (int16_t)raw_value;
        break;

    case TYPE_U16:
        *value = (uint16_t)raw_value;
        break;

    case TYPE_I32:
        *value = (int32_t)(uint32_t)raw_value;
        break;

    case TYPE_U32:
    case TYPE_BIT_FIELD:
    case TYPE_BCD_TIME:
        *value = (int32_t)(uint32_t)raw_value;
        break;

    case TYPE_FLOAT32:
        raw_float = (uint32_t)raw_value;
        rt_memcpy(&float32_value, &raw_float, sizeof(float32_value));
        scaled_value = (double)float32_value * inv_data_decimal_scale(point->decimal_places);

        /* 浮点定点化结果超过int32_t上限时按最大值保存。 */
        if(scaled_value > INT32_MAX) {
            *value = INT32_MAX;
        }
        /* 浮点定点化结果低于int32_t下限时按最小值保存。 */
        else if(scaled_value < INT32_MIN) {
            *value = INT32_MIN;
        }
        /* 浮点定点化结果位于int32_t范围内时直接转换。 */
        else {
            *value = (int32_t)scaled_value;
        }
        break;

    case TYPE_FLOAT64:
        rt_memcpy(&float64_value, &raw_value, sizeof(float64_value));
        scaled_value = float64_value * inv_data_decimal_scale(point->decimal_places);

        /* 双精度定点化结果超过int32_t上限时按最大值保存。 */
        if(scaled_value > INT32_MAX) {
            *value = INT32_MAX;
        }
        /* 双精度定点化结果低于int32_t下限时按最小值保存。 */
        else if(scaled_value < INT32_MIN) {
            *value = INT32_MIN;
        }
        /* 双精度定点化结果位于int32_t范围内时直接转换。 */
        else {
            *value = (int32_t)scaled_value;
        }
        break;

    case TYPE_BCD:
        return inv_data_decode_bcd(bytes, byte_count, value);

    case TYPE_ASCII:
    default:
        *value = (int32_t)(uint32_t)raw_value;
        break;
    }

    return RT_TRUE;
}

/* 将设备编号响应保存为字符串，非ASCII类型先转换为int32_t十进制文本。 */
static rt_bool_t inv_data_store_string(const Inv_Data_Point_Config_t *point,
                                       const uint16_t *registers)
{
    Inv_RealtimeString_t *target = point->string_target;
    uint8_t bytes[INV_DATA_DEVICE_NO_MAX_LEN];
    uint16_t byte_count;
    uint16_t index;

    /* 字符串目标为空时不能保存设备编号。 */
    if(target == RT_NULL) {
        return RT_FALSE;
    }

    rt_memset(target->value, 0, sizeof(target->value));

    /* ASCII类型按照协议字节序直接复制可见内容。 */
    if(point->data_type == TYPE_ASCII) {
        byte_count = inv_data_registers_to_bytes(registers, point->reg_count, point->byte_order, bytes, sizeof(bytes));

        /* 遇到字符串结束符或Flash空白值时结束设备编号。 */
        for(index = 0U; index < byte_count; ++index) {
            /* 当前字节是结束符或擦除值时停止复制。 */
            if((bytes[index] == 0U) || (bytes[index] == 0xFFU)) {
                break;
            }

            target->value[index] = (char)bytes[index];
        }

        target->length = (uint8_t)index;
    }
    /* 整数或BCD设备编号统一转换为十进制字符串。 */
    else {
        int32_t number;

        /* 数值解析失败时设备编号保持无效。 */
        if(inv_data_decode_number(point, registers, &number) == RT_FALSE) {
            return RT_FALSE;
        }

        target->length = (uint8_t)rt_snprintf(target->value, sizeof(target->value), "%d", number);
    }

    target->update_tick = rt_tick_get();
    target->valid = 1U;
    return RT_TRUE;
}

/* 将一个数据点对应的寄存器数据写入当前档案实时数据。 */
static rt_bool_t inv_data_store_point_value(const Inv_Data_Point_Config_t *point,
                                            const uint16_t *registers)
{
    int32_t value;

    /* 字符串目标存在时按设备编号规则保存。 */
    if(point->string_target != RT_NULL) {
        return inv_data_store_string(point, registers);
    }

    /* 数值目标为空或数值解析失败时不能更新实时数据。 */
    if((point->number_target == RT_NULL) ||
       (inv_data_decode_number(point, registers, &value) == RT_FALSE)) {
        return RT_FALSE;
    }

    point->number_target->value = value;
    point->number_target->update_tick = rt_tick_get();
    point->number_target->valid = 1U;
    return RT_TRUE;
}

/* 按各数据点寄存器数量拆分合并响应，并分别写入对应的实时数据。 */
static rt_bool_t inv_data_store_active_values(Inv_Data_Port_Context_t *context,
                                              const uint16_t *registers)
{
    uint16_t register_offset = 0U;
    uint8_t point_index;

    /* 普通请求循环一次，三相连续请求最多循环三次。 */
    for(point_index = 0U; point_index < context->active_point_count; ++point_index) {
        Inv_Data_Point_Config_t *point = inv_data_get_active_point(context, point_index);

        /* 任意数据点转换失败时整批响应按失败处理，防止部分数据被误认为本轮有效。 */
        if(inv_data_store_point_value(point, &registers[register_offset]) == RT_FALSE) {
            return RT_FALSE;
        }

        register_offset += point->reg_count;
    }

    return RT_TRUE;
}

/* 查找当前端口下一数据点、组成Modbus请求并写入对应串口。 */
static void inv_data_send_next_request(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[MODBUS_READ_REQUEST_LEN];
    uint16_t frame_len = 0U;
    rt_size_t written_size;
    char frame_name[96];

    /* 当前端口没有有效档案或可读寄存器时保持READY，等待下一次调度重新检查。 */
    if(inv_data_find_next_point(context) == RT_FALSE) {
        return;
    }

    inv_data_combine_phase_points(context); /* 仅尝试合并地址连续的三相电压或三相电流数据点。 */

    /* 当前寄存器配置无法组成合法Modbus请求时将该数据点置为无效。 */
    if(modbus_m_read_request(context->slave_addr,
                             context->active_point.function_code,
                             context->active_point.reg_addr,
                             context->active_reg_count,
                             frame,
                             sizeof(frame),
                             &frame_len) != RT_EOK) {
        rt_kprintf("[%08d] uart[%d] archive[%d] could not build request for data[%s]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_point.name);
        inv_data_invalidate_active_point(context);
        inv_data_finish_active_point(context);
        return;
    }

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] addr[%d] data[%s] values[%d] request", context->uart_no, context->active_archive_index + 1, context->slave_addr, context->active_point.name, context->active_point_count);
    show_arr(frame_name, frame, frame_len);
    written_size = uart_mgmt_write(context->uart_no, frame, frame_len);

    /* 请求没有完整写入串口时不启动响应超时，下一次调度继续读取后续数据点。 */
    if(written_size != frame_len) {
        rt_kprintf("[%08d] uart[%d] archive[%d] could not send full data[%s] request, expected[%d], sent[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_point.name, frame_len, written_size);
        inv_data_invalidate_active_point(context);
        inv_data_finish_active_point(context);
        return;
    }

    context->request_tick = rt_tick_get();
    context->request_type = INV_DATA_REQUEST_PERIODIC_READ;
    context->state = INV_DATA_PORT_WAIT_RESPONSE;
}

/* 从端口接收邮箱安全取出一帧，复制完成后立即释放邮箱。 */
static uint16_t inv_data_take_rx_frame(Inv_Data_Port_Context_t *context, uint8_t *frame)
{
    rt_base_t level;
    uint16_t frame_len;

    level = rt_hw_interrupt_disable();
    frame_len = context->rx_frame_len;
    rt_memcpy(frame, context->rx_frame, frame_len);
    context->rx_ready = RT_FALSE;
    rt_hw_interrupt_enable(level);
    return frame_len;
}

/* 解析当前端口收到的响应，成功时更新实时数据，失败时立即进入下一数据点。 */
static void inv_data_handle_response(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[INV_DATA_RX_FRAME_SIZE];
    uint16_t registers[MODBUS_READ_REG_MAX];
    uint16_t frame_len;
    uint16_t register_count = 0U;
    uint8_t exception_code = 0U;
    modbus_m_parse_result result;
    char frame_name[112];

    frame_len = inv_data_take_rx_frame(context, frame);
    result = modbus_m_read_response(context->slave_addr,
                                    context->active_point.function_code,
                                    context->active_reg_count,
                                    frame,
                                    frame_len,
                                    registers,
                                    MODBUS_READ_REG_MAX,
                                    &register_count,
                                    &exception_code);
    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] data[%s] values[%d] reply[%s]", context->uart_no, context->active_archive_index + 1, context->active_point.name, context->active_point_count, modbus_m_parse_result_text(result));
    show_arr(frame_name, frame, frame_len);

    /* 报文解析成功并且实时数据转换成功时打印本次保存结果。 */
    if((result == MODBUS_M_PARSE_OK) &&
       (register_count == context->active_reg_count) &&
       (inv_data_store_active_values(context, registers) == RT_TRUE)) {
        rt_kprintf("[%08d] uart[%d] archive[%d] saved data[%s], values[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_point.name, context->active_point_count);
    }
    /* 收到报文但解析或数据转换失败时，本次请求立即结束，不再继续计算超时。 */
    else {
        rt_kprintf("[%08d] uart[%d] archive[%d] data[%s] reply rejected: %s, exception[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_point.name, modbus_m_parse_result_text(result), exception_code);
        inv_data_invalidate_active_point(context);
    }

    inv_data_finish_active_point(context);
}

/* 控制事务结束后恢复写请求插入前的周期抄读状态。 */
static void inv_control_finish(Inv_Data_Port_Context_t *context)
{
    context->request_type = INV_DATA_REQUEST_NONE;
    context->state = context->resume_state;
}

/* 从当前端口队列取出一项控制请求，转换控制值并发送Modbus写报文。 */
static void inv_control_send_next_request(Inv_Data_Port_Context_t *context)
{
    Inv_Control_Request_t request;
    Inv_Control_Result_t result;
    uint8_t frame[MODBUS_RTU_ADU_MAX];
    uint16_t frame_len = 0U;
    rt_size_t written_size;
    char frame_name[112];

    /* 队列可能在状态判断后被其他线程改变，实际取不到请求时直接返回。 */
    if(inv_control_queue_get(&context->control_queue, &request) == RT_FALSE) {
        return;
    }

    result = inv_control_get_active(&request, &context->active_control);

    /* 档案或协议控制配置无效时生成结果，不占用串口等待状态。 */
    if(result != INV_CONTROL_RESULT_OK) {
        inv_control_push_result(&request, result, 0U);
        rt_kprintf("[%08d] uart[%d] archive[%d] control type[%d] cannot run, result[%d]\n", rt_tick_get(), context->uart_no, request.archive_index + 1, request.type, result);
        return;
    }

    context->active_archive_index = request.archive_index;
    context->slave_addr = g_inv_archive_lib.archives[request.archive_index].mb_addr;

    /* 控制值必须先按照协议数据类型和字节序转换为线上寄存器数据。 */
    if(inv_control_value_to_registers(&context->active_control) == RT_FALSE) {
        inv_control_push_result(&context->active_control.request, INV_CONTROL_RESULT_BUILD_FAILED, 0U);
        rt_kprintf("[%08d] uart[%d] archive[%d] control[%s] value[%d] cannot be converted\n", rt_tick_get(), context->uart_no, request.archive_index + 1, context->active_control.name, context->active_control.request.value);
        return;
    }

    /* 根据协议库配置的06或10功能码组成实际下行控制请求。 */
    if(modbus_m_write_request(context->slave_addr,
                              context->active_control.function_code,
                              context->active_control.reg_addr,
                              context->active_control.registers,
                              context->active_control.reg_count,
                              frame,
                              sizeof(frame),
                              &frame_len) != RT_EOK) {
        inv_control_push_result(&context->active_control.request, INV_CONTROL_RESULT_BUILD_FAILED, 0U);
        rt_kprintf("[%08d] uart[%d] archive[%d] could not build control[%s] request\n", rt_tick_get(), context->uart_no, request.archive_index + 1, context->active_control.name);
        return;
    }

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] addr[%d] control[%s] request", context->uart_no, request.archive_index + 1, context->slave_addr, context->active_control.name);
    show_arr(frame_name, frame, frame_len);
    written_size = uart_mgmt_write(context->uart_no, frame, frame_len);

    /* 控制请求没有完整写入串口时立即生成失败结果，不启动1秒响应超时。 */
    if(written_size != frame_len) {
        inv_control_push_result(&context->active_control.request, INV_CONTROL_RESULT_SEND_FAILED, 0U);
        rt_kprintf("[%08d] uart[%d] archive[%d] could not send full control[%s] request, expected[%d], sent[%d]\n", rt_tick_get(), context->uart_no, request.archive_index + 1, context->active_control.name, frame_len, written_size);
        return;
    }

    /* 写请求发送成功后保存原状态，写事务结束再继续原来的周期抄读或10秒空闲。 */
    context->resume_state = context->state;
    context->request_tick = rt_tick_get();
    context->request_type = INV_DATA_REQUEST_CONTROL_WRITE;
    context->state = INV_DATA_PORT_WAIT_RESPONSE;
}

/* 解析当前控制写响应，生成异步结果并恢复该端口原来的周期状态。 */
static void inv_control_handle_response(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[INV_DATA_RX_FRAME_SIZE];
    uint16_t frame_len;
    uint8_t exception_code = 0U;
    modbus_m_parse_result parse_result;
    Inv_Control_Result_t control_result;
    char frame_name[112];

    frame_len = inv_data_take_rx_frame(context, frame);
    parse_result = modbus_m_write_response(context->slave_addr,
                                           context->active_control.function_code,
                                           context->active_control.reg_addr,
                                           context->active_control.registers,
                                           context->active_control.reg_count,
                                           frame,
                                           frame_len,
                                           &exception_code);
    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] control[%s] reply[%s]", context->uart_no, context->active_archive_index + 1, context->active_control.name, modbus_m_parse_result_text(parse_result));
    show_arr(frame_name, frame, frame_len);

    /* 完整写响应校验通过时更新控制实时值，并向调用方返回成功结果。 */
    if(parse_result == MODBUS_M_PARSE_OK) {
        context->active_control.target->value = context->active_control.request.value;
        context->active_control.target->update_tick = rt_tick_get();
        context->active_control.target->valid = 1U;
        control_result = INV_CONTROL_RESULT_OK;
        rt_kprintf("[%08d] uart[%d] archive[%d] control[%s] finished, value[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_control.name, context->active_control.request.value);
    }
    /* 从站异常响应需要保留异常码，便于上层转换成自己的应答状态。 */
    else if(parse_result == MODBUS_M_PARSE_EXCEPTION) {
        control_result = INV_CONTROL_RESULT_DEVICE_EXCEPTION;
        rt_kprintf("[%08d] uart[%d] archive[%d] control[%s] device exception[%d]\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_control.name, exception_code);
    }
    /* 已收到但不能匹配本次写请求的报文直接结束，不再继续计算超时。 */
    else {
        control_result = INV_CONTROL_RESULT_RESPONSE_INVALID;
        rt_kprintf("[%08d] uart[%d] archive[%d] control[%s] reply rejected: %s\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->active_control.name, modbus_m_parse_result_text(parse_result));
    }

    inv_control_push_result(&context->active_control.request, control_result, exception_code);
    inv_control_finish(context);
}

/* 写请求1秒内没有收到任何报文时生成超时结果，并恢复原周期状态。 */
static void inv_control_handle_timeout(Inv_Data_Port_Context_t *context)
{
    rt_kprintf("[%08d] uart[%d] archive[%d] addr[%d] control[%s] no reply within 1s\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->slave_addr, context->active_control.name);
    inv_control_push_result(&context->active_control.request, INV_CONTROL_RESULT_TIMEOUT, 0U);
    inv_control_finish(context);
}

/* 每次调度只推进一个端口状态机的一步，三个端口之间不会互相等待。 */
static void inv_data_process_port(Inv_Data_Port_Context_t *context, rt_tick_t now)
{
    /* 等待响应时必须先结束当前线上事务，排队的实时控制不能破坏读写应答对应关系。 */
    if(context->state == INV_DATA_PORT_WAIT_RESPONSE) {
        /* 收到完整响应时按当前请求类型解析，本次请求不再参与超时判断。 */
        if(context->rx_ready == RT_TRUE) {
            if(context->request_type == INV_DATA_REQUEST_CONTROL_WRITE) {
                inv_control_handle_response(context);
            }
            else {
                inv_data_handle_response(context);
            }
            return;
        }

        /* 只有没有收到报文并且等待达到1秒时，才结束当前读写事务。 */
        if((rt_tick_t)(now - context->request_tick) >= INV_DATA_RESPONSE_TIMEOUT_TICKS) {
            if(context->request_type == INV_DATA_REQUEST_CONTROL_WRITE) {
                inv_control_handle_timeout(context);
            }
            else {
                rt_kprintf("[%08d] uart[%d] archive[%d] addr[%d] data[%s] no reply within 1s\n", rt_tick_get(), context->uart_no, context->active_archive_index + 1, context->slave_addr, context->active_point.name);
                inv_data_invalidate_active_point(context);
                inv_data_finish_active_point(context);
            }
        }
        return;
    }

    /* 当前端口没有线上事务时优先执行控制，避免10秒设备空闲延迟实时命令。 */
    if(inv_control_queue_has_request(&context->control_queue) == RT_TRUE) {
        inv_control_send_next_request(context);
        return;
    }

    /* 设备空闲状态达到10秒后恢复READY，下一次调度开始读取下一台逆变器。 */
    if(context->state == INV_DATA_PORT_DEVICE_IDLE) {
        if((rt_tick_t)(now - context->idle_tick) >= INV_DATA_DEVICE_IDLE_TICKS) {
            context->state = INV_DATA_PORT_READY;
            rt_kprintf("[%08d] uart[%d] 10s wait finished, continue reading\n", rt_tick_get(), context->uart_no);
        }
        return;
    }

    /* READY状态查找并发送当前端口的下一项寄存器请求。 */
    if(context->state == INV_DATA_PORT_READY) {
        inv_data_send_next_request(context);
    }
}

/* 初始化实时数据及三个端口状态机，端口映射与自动识别阶段保持一致。 */
void Inv_Data_Init(void)
{
    g_inv_data_initialized = RT_FALSE;
    rt_memset(g_inv_data, 0, sizeof(g_inv_data));
    rt_memset(g_inv_data_ports, 0, sizeof(g_inv_data_ports));
    rt_memset(g_inv_control_results, 0, sizeof(g_inv_control_results));
    g_inv_control_result_read_index = 0U;
    g_inv_control_result_write_index = 0U;
    g_inv_control_result_count = 0U;

    g_inv_data_ports[0].uart_no         = UART1_NO;
    g_inv_data_ports[0].archive_port    = INV_PORT_RS485_2;
    g_inv_data_ports[1].uart_no         = UART3_NO;
    g_inv_data_ports[1].archive_port    = INV_PORT_RJ45_1;
    g_inv_data_ports[2].uart_no         = UART5_NO;
    g_inv_data_ports[2].archive_port    = INV_PORT_RJ45_2;
    g_inv_data_initialized = RT_TRUE;
}

/* 周期抄读主循环顺序推进三个独立状态机，不会串行等待某一路响应超时。 */
void Inv_Data_Poll_Loop(void)
{
    uint8_t index;

    Inv_Data_Init();

    /* 周期抄读线程持续运行，档案变化后下一轮会自动重新检查有效槽位。 */
    while(1) {
        rt_tick_t now = rt_tick_get();

        /* 每次调度分别推进三个端口，不在某个端口内部阻塞等待响应。 */
        for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
            inv_data_process_port(&g_inv_data_ports[index], now);
        }

        rt_thread_mdelay(INV_DATA_POLL_TICKS);
    }
}

/* 串口管理层提交周期读取或控制写响应，每个端口使用自己的单帧接收邮箱。 */
rt_err_t Inv_Data_Rx_Frame(uint16_t uart_no, const uint8_t *frame, uint16_t frame_len)
{
    Inv_Data_Port_Context_t *context = RT_NULL;
    rt_base_t level;
    uint8_t index;

    /* 报文指针为空、长度为0或超过接收邮箱容量时拒绝接收。 */
    if((frame == RT_NULL) || (frame_len == 0U) || (frame_len > INV_DATA_RX_FRAME_SIZE)) {
        return -RT_EINVAL;
    }

    /* 按串口编号查找对应的下行读写端口上下文。 */
    for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
        /* 串口编号匹配时保存对应上下文并结束查找。 */
        if(g_inv_data_ports[index].uart_no == uart_no) {
            context = &g_inv_data_ports[index];
            break;
        }
    }

    /* 非下行管理串口或当前端口没有等待读写响应时不接收报文。 */
    if((context == RT_NULL) || (context->state != INV_DATA_PORT_WAIT_RESPONSE)) {
        return -RT_EBUSY;
    }

    level = rt_hw_interrupt_disable();

    /* 上一帧尚未处理时禁止覆盖该端口的单帧邮箱。 */
    if(context->rx_ready == RT_TRUE) {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;
    }

    rt_memcpy(context->rx_frame, frame, frame_len);
    context->rx_frame_len = frame_len;
    context->rx_ready = RT_TRUE;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/* 异步提交一项逆变器控制请求，同一物理端口按照提交顺序依次执行。 */
rt_err_t Inv_Control_Submit(const Inv_Control_Request_t *request)
{
    Inv_Data_Port_Context_t *context;
    rt_base_t level;

    /* 周期线程尚未初始化、请求为空或参数越界时不能进入控制队列。 */
    if(g_inv_data_initialized != RT_TRUE) {
        return -RT_EBUSY;
    }

    if((request == RT_NULL) || (request->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (request->type < INV_CONTROL_POWER_ON) || (request->type >= INV_CONTROL_TYPE_MAX)) {
        return -RT_EINVAL;
    }

    context = inv_control_find_port_context(request->archive_index);

    /* 无效档案或无法映射到三个下行端口时拒绝提交。 */
    if(context == RT_NULL) {
        return -RT_EINVAL;
    }

    level = rt_hw_interrupt_disable();

    /* 每个端口最多暂存4项控制请求，队列满时由调用方稍后重试。 */
    if(context->control_queue.count >= INV_CONTROL_QUEUE_SIZE) {
        rt_hw_interrupt_enable(level);
        return -RT_EFULL;
    }

    context->control_queue.requests[context->control_queue.write_index] = *request;
    context->control_queue.write_index = (context->control_queue.write_index + 1U) % INV_CONTROL_QUEUE_SIZE;
    ++context->control_queue.count;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[%08d] archive[%d] control type[%d] request[%d] queued on uart[%d]\n", rt_tick_get(), request->archive_index + 1, request->type, request->request_id, context->uart_no);
    return RT_EOK;
}

/* 取出最早生成的一项控制结果，查询和周期线程写入之间使用临界区保护。 */
rt_err_t Inv_Control_Get_Result(Inv_Control_Result_Info_t *result)
{
    rt_base_t level;

    /* 输出指针为空时不能返回控制结果。 */
    if(result == RT_NULL) {
        return -RT_EINVAL;
    }

    level = rt_hw_interrupt_disable();

    /* 结果队列为空表示当前没有已经结束的控制事务。 */
    if(g_inv_control_result_count == 0U) {
        rt_hw_interrupt_enable(level);
        return -RT_EEMPTY;
    }

    *result = g_inv_control_results[g_inv_control_result_read_index];
    g_inv_control_result_read_index = (g_inv_control_result_read_index + 1U) % INV_CONTROL_RESULT_QUEUE_SIZE;
    --g_inv_control_result_count;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/* 按档案槽位获取实时数据，无效档案或越界时返回RT_NULL。 */
Inv_Data_t *Inv_Data_Get(uint8_t archive_index)
{
    /* 只有下标有效并且对应档案当前有效时才返回实时数据地址。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID)) {
        return RT_NULL;
    }

    return &g_inv_data[archive_index];
}
