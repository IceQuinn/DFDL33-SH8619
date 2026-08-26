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
#include "user_rtc.h"

#define INV_DATA_PORT_COUNT                3U
#define INV_DATA_POINT_COUNT              31U
#define INV_DATA_RESPONSE_TIMEOUT_TICKS  1000U
#define INV_DATA_DEVICE_IDLE_TICKS      10000U
#define INV_DATA_POLL_TICKS               10U
#define INV_DATA_TIME_CHECK_TICKS        1000U
#define INV_DATA_START_HOUR                  7
#define INV_DATA_STOP_HOUR                  17
#define INV_DATA_RX_FRAME_SIZE            MODBUS_RTU_ADU_MAX
#define INV_DATA_NUMERIC_BYTE_MAX          8U
#define INV_DATA_PHASE_COMBINE_MAX         3U
#define INV_CONTROL_QUEUE_SIZE             4U
#define INV_CONTROL_RESULT_QUEUE_SIZE     16U
#define INV_CONTROL_REGISTER_MAX          (INV_DATA_NUMERIC_BYTE_MAX / 2U)

/* 每个端口的抄读状态独立变化，等待某一路响应时其他端口仍可继续发送或解析。 */
typedef enum Inv_Data_Port_State {
    INV_DATA_PORT_READY = 0,              /* 当前端口可以查找并发送下一项寄存器请求。 */
    INV_DATA_PORT_DEVICE_IDLE,            /* 当前端口完成一台逆变器抄读，正在空闲10秒。 */
    INV_DATA_PORT_WAIT_PERIODIC_READ,     /* 当前端口正在等待周期抄读响应。 */
    INV_DATA_PORT_WAIT_CONTROL_WRITE,     /* 当前端口正在等待实时控制写响应。 */
    INV_DATA_PORT_WAIT_CONTROL_REFRESH    /* 当前端口正在等待控制寄存器优先回读响应。 */
} Inv_Data_Port_State_t;

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

/* 周期抄读活动数据只在等待周期读响应期间有效，普通请求也统一放在points[0]。 */
typedef struct Inv_Data_Read_Active {
    Inv_Data_Point_Config_t points[INV_DATA_PHASE_COMBINE_MAX]; /* 当前请求合并的数据点配置，最多包含三相数据。 */
    uint8_t point_count;                                        /* 当前请求合并的数据点数量，普通请求固定为1。 */
    uint16_t reg_count;                                         /* 当前请求包含的寄存器总数，用于组帧及校验响应长度。 */
    rt_bool_t idle_after_active;                                /* RT_TRUE表示当前数据点完成后需要进入10秒空闲。 */
} Inv_Data_Read_Active_t;

/* 周期读与控制事务不会同时占用同一串口，使用联合体复用两类活动数据内存。 */
typedef union Inv_Data_Active {
    Inv_Data_Read_Active_t read;       /* 周期抄读及连续三相数据合并使用的活动数据。 */
    Inv_Control_Active_t control;      /* 控制写和控制寄存器优先回读使用的活动数据。 */
} Inv_Data_Active_t;

/* 串口编号和档案端口映射固定不变，放入只读表后无需在每个运行上下文中重复保存。 */
typedef struct Inv_Data_Port_Config {
    uint16_t uart_no;                  /* 串口管理模块使用的逻辑串口编号。 */
    uint8_t archive_port;              /* 当前串口对应的逆变器档案接入端口。 */
} Inv_Data_Port_Config_t;

/* 单个端口上下文保存独立档案游标、当前请求、超时tick及接收邮箱。 */
typedef struct Inv_Data_Port_Context {
    uint8_t port_index;                      /* 只读端口配置表下标，范围0～2。 */
    uint8_t archive_index;                   /* 下一次查找数据点使用的档案槽位游标。 */
    uint8_t point_index;                     /* 下一次查找使用的数据点下标，范围0～30。 */
    uint8_t active_archive_index;            /* 当前已发送请求所属的档案槽位下标。 */
    uint8_t slave_addr;                      /* 当前已发送请求使用的Modbus从站地址。 */
    Inv_Data_Port_State_t state;             /* 当前端口处于可发送、等待响应或空闲状态。 */
    Inv_Data_Port_State_t resume_state;      /* 实时控制完成后需要恢复的周期抄读状态。 */
    Inv_Data_Active_t active;                /* 当前线上事务使用的周期读或控制活动数据。 */
    Inv_Control_Queue_t control_queue;       /* 当前端口等待发送的高优先级控制请求。 */
    uint8_t control_refresh_mask[INVERTER_ARCHIVE_MAX_COUNT]; /* 各档案待优先回读的控制类型位图。 */
    rt_bool_t control_refresh_pending;       /* RT_TRUE表示当前端口至少存在一项控制寄存器回读标志。 */
    rt_tick_t request_tick;                  /* 当前请求完整写入串口时记录的系统tick。 */
    rt_tick_t idle_tick;                     /* 当前逆变器完成全部抄读并进入空闲时的系统tick。 */
    uint8_t rx_frame[INV_DATA_RX_FRAME_SIZE]; /* 当前端口接收到的完整Modbus响应报文。 */
    uint16_t rx_frame_len;                   /* rx_frame缓冲区中的有效响应报文长度。 */
    volatile rt_bool_t rx_ready;             /* RT_TRUE表示接收邮箱中存在待解析响应。 */
} Inv_Data_Port_Context_t;

/* 三个物理串口与档案接入端口保持固定映射，运行上下文只需保存本表下标。 */
static const Inv_Data_Port_Config_t g_inv_data_port_configs[INV_DATA_PORT_COUNT] = {
        {UART6_NO, INV_PORT_RS485_2},       /* UART1连接RS485-II。 */
        {UART7_NO, INV_PORT_RJ45_1},        /* UART3连接RJ45-I。 */
        {UART4_NO, INV_PORT_RJ45_2},        /* UART5连接RJ45-II。 */
};

/* 实时数据数组下标与档案槽位下标固定对应，数据不写入Flash。 */
Inv_Data_t g_inv_data[INVERTER_ARCHIVE_MAX_COUNT]; /* 12个档案槽位对应的数据、参数和控制实时值。 */

/* 三个有线端口分别维护一套完全独立的周期抄读状态。 */
static Inv_Data_Port_Context_t g_inv_data_ports[INV_DATA_PORT_COUNT];

/* 控制结果使用独立环形队列，后续645模块可以通过查询接口异步取走。 */
static Inv_Control_Result_Info_t g_inv_control_results[INV_CONTROL_RESULT_QUEUE_SIZE]; /* 保存待上层读取的异步控制结果。 */
static uint8_t g_inv_control_result_read_index;  /* 下一项待取控制结果下标。 */
static uint8_t g_inv_control_result_write_index; /* 下一项待写控制结果下标。 */
static uint8_t g_inv_control_result_count;       /* 当前结果队列中的有效结果数量。 */
static struct rt_semaphore g_inv_control_result_sem; /* 控制结果到达时唤醒等待结果的上层线程。 */
static rt_bool_t g_inv_control_result_sem_initialized; /* RT_TRUE表示结果信号量已经初始化。 */
static volatile rt_bool_t g_inv_data_initialized; /* RT_TRUE表示端口上下文已经完成初始化。 */
static volatile rt_bool_t g_inv_data_work_enabled; /* RT_TRUE表示当前处于07:00～17:00工作时段。 */
static rt_bool_t g_inv_data_work_state_initialized; /* RT_TRUE表示已经完成首次工作时段判断。 */

/* 数据类各数组对应的日志名称。 */
static const char *g_inv_data_u_name[ENUM_PHASE_MAX] = {"Ua", "Ub", "Uc"}; /* 三相电压点日志名称。 */
static const char *g_inv_data_i_name[ENUM_PHASE_MAX] = {"Ia", "Ib", "Ic"}; /* 三相电流点日志名称。 */
static const char *g_inv_data_p_name[ENUM_PMAX] = {"Pa", "Pb", "Pc", "Pt"}; /* 分相及总有功功率日志名称。 */
static const char *g_inv_data_q_name[ENUM_QMAX] = {"Qa", "Qb", "Qc", "Qt"}; /* 分相及总无功功率日志名称。 */
static const char *g_inv_data_pf_name[ENUM_PFMAX] = {"PFa", "PFb", "PFc", "PFt"}; /* 分相及总功率因数日志名称。 */

/* 从只读端口配置表取得当前状态机对应的串口编号。 */
static uint16_t inv_data_uart_no(const Inv_Data_Port_Context_t *context)
{
    return g_inv_data_port_configs[context->port_index].uart_no;
}

/* 从只读端口配置表取得当前状态机对应的档案接入端口。 */
static uint8_t inv_data_archive_port(const Inv_Data_Port_Context_t *context)
{
    return g_inv_data_port_configs[context->port_index].archive_port;
}

/* 三种等待状态都表示串口已有在线事务，接收层只在这些状态下接收响应。 */
static rt_bool_t inv_data_state_waiting_response(Inv_Data_Port_State_t state)
{
    /* 周期读、控制写和控制回读三种状态都已经占用串口并等待对应响应。 */
    if((state == INV_DATA_PORT_WAIT_PERIODIC_READ) ||
       (state == INV_DATA_PORT_WAIT_CONTROL_WRITE) ||
       (state == INV_DATA_PORT_WAIT_CONTROL_REFRESH)) {
        return RT_TRUE;
    }

    return RT_FALSE;
}

/* 根据RTC本地小时判断当前是否处于07:00:00～16:59:59周期工作时段。 */
static rt_bool_t inv_data_work_time_active(void)
{
    struct tm local_time = get_local_time_t(); /* 从RTC取得已经转换为本地时区的日历时间。 */

    return ((local_time.tm_hour >= INV_DATA_START_HOUR) &&
            (local_time.tm_hour < INV_DATA_STOP_HOUR)) ? RT_TRUE : RT_FALSE;
}

/* 更新时间窗口总开关，只在首次检查或运行状态变化时打印一次日志。 */
static void inv_data_update_work_state(void)
{
    rt_bool_t work_enabled = inv_data_work_time_active(); /* 本次RTC检查得到的工作时段状态。 */
    rt_bool_t state_changed;                             /* 本次状态是否与上次记录不同。 */
    rt_base_t level;                                     /* 修改跨线程状态前保存的中断级别。 */

    level = rt_hw_interrupt_disable(); /* 原子更新工作开关及首次初始化标志。 */
    state_changed = ((g_inv_data_work_state_initialized != RT_TRUE) ||
                     (g_inv_data_work_enabled != work_enabled)) ? RT_TRUE : RT_FALSE;
    g_inv_data_work_enabled = work_enabled;
    g_inv_data_work_state_initialized = RT_TRUE;
    rt_hw_interrupt_enable(level); /* 工作时段共享状态更新完成后恢复中断。 */

    /* 状态没有变化时不重复打印，避免每秒检查RTC产生相同日志。 */
    if(state_changed != RT_TRUE) {
        return;
    }

    /* 进入07:00～17:00窗口时打印一次启动日志。 */
    if(work_enabled == RT_TRUE) {
        rt_kprintf("%s inverter periodic reading started, active time[07:00-17:00]\n", get_char_time());
    }
    /* 离开工作窗口时打印一次停止日志，已在线事务仍由状态机收尾。 */
    else {
        rt_kprintf("%s inverter periodic reading stopped, active time[07:00-17:00]\n", get_char_time());
    }
}

/* 按档案接入端口查找对应的周期抄读端口上下文。 */
static Inv_Data_Port_Context_t *inv_control_find_port_context(uint8_t archive_index)
{
    uint8_t archive_port; /* 目标档案记录的物理接入端口。 */
    uint8_t index;        /* 三个周期抄读端口的遍历下标。 */

    /* 档案下标越界或档案当前无效时不能查找下行端口。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID)) {
        return RT_NULL;
    }

    archive_port = g_inv_archive_lib.archives[archive_index].port; /* 保存端口号以便匹配只读映射表。 */

    /* 三个端口配置的archive_port与档案中的接入端口使用同一套编号。 */
    for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
        /* 映射表端口号与档案端口号一致时返回该端口的独立状态机。 */
        if(inv_data_archive_port(&g_inv_data_ports[index]) == archive_port) {
            return &g_inv_data_ports[index];
        }
    }

    return RT_NULL;
}

/* 从端口控制队列安全取出最早提交的一项请求。 */
static rt_bool_t inv_control_queue_get(Inv_Control_Queue_t *queue, Inv_Control_Request_t *request)
{
    rt_base_t level; /* 读取环形队列前保存的中断级别。 */

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
    rt_base_t level;       /* 读取队列计数前保存的中断级别。 */
    rt_bool_t has_request; /* 当前端口队列是否至少包含一项控制请求。 */

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
    Inv_Control_Result_Info_t result_info;  /* 即将写入公共结果队列的完整结果。 */
    rt_base_t level;                        /* 更新公共结果队列前保存的中断级别。 */
    rt_bool_t release_result_sem = RT_TRUE; /* 是否需要为新增结果增加一次信号量计数。 */

    rt_memset(&result_info, 0, sizeof(result_info)); /* 清除结果结构的保留内容。 */
    result_info.request = *request;
    result_info.result = result;
    result_info.exception_code = exception_code;
    result_info.finish_tick = rt_tick_get(); /* 记录下行控制流程结束时刻供上层诊断。 */

    level = rt_hw_interrupt_disable();

    /* 队列已满时用最新结果替换最旧结果，结果总数不变，因此不增加信号量计数。 */
    if(g_inv_control_result_count >= INV_CONTROL_RESULT_QUEUE_SIZE) {
        g_inv_control_result_read_index = (g_inv_control_result_read_index + 1U) % INV_CONTROL_RESULT_QUEUE_SIZE;
        --g_inv_control_result_count;
        release_result_sem = RT_FALSE;
    }

    g_inv_control_results[g_inv_control_result_write_index] = result_info;
    g_inv_control_result_write_index = (g_inv_control_result_write_index + 1U) % INV_CONTROL_RESULT_QUEUE_SIZE;
    ++g_inv_control_result_count;
    rt_hw_interrupt_enable(level);

    /* 新增结果后释放一次信号量，正在等待的上层线程会立即被唤醒。 */
    if(release_result_sem == RT_TRUE) {
        rt_sem_release(&g_inv_control_result_sem); /* 唤醒一个正在等待控制结果的上层线程。 */
    }
}

/* 将只读协议寄存器复制为统一抄读配置，避免直接读取1字节对齐结构中的16位字段。 */
static void inv_data_copy_read_config(Inv_Data_Point_Config_t *point, const Inv_RegBlk_t *reg)
{
    Inv_RegBlk_t local_reg; /* 对齐副本，避免直接访问紧凑结构中的16位字段。 */

    rt_memcpy(&local_reg, reg, sizeof(local_reg)); /* 通过字节复制安全取得协议库配置。 */
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
    Inv_CtrlRegBlk_t local_reg; /* 普通控制寄存器配置的对齐副本。 */

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
    Inv_CtrlDefaultRegBlk_t local_reg; /* 带默认写入值控制配置的对齐副本。 */

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
    const Inv_Proto_t *protocol;         /* 目标档案绑定的厂家协议库。 */
    Inv_CtrlRegBlk_t ctrl_reg;           /* 数值类控制寄存器的对齐副本。 */
    Inv_CtrlDefaultRegBlk_t default_reg; /* 开关机控制寄存器的对齐副本。 */

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

    protocol = Inv_Archive_Get_Protocol(request->archive_index); /* 取得组写报文所需的控制寄存器配置。 */

    /* 有效档案没有匹配协议指针时不能确定控制寄存器。 */
    if(protocol == RT_NULL) {
        return INV_CONTROL_RESULT_PROTOCOL_MISSING;
    }

    rt_memset(active, 0, sizeof(*active)); /* 防止上一次控制事务的配置残留。 */
    active->request = *request;

    /* 开关机控制使用协议库中各自配置的固定默认写入值。 */
    if((request->type == INV_CONTROL_POWER_ON) || (request->type == INV_CONTROL_POWER_OFF)) {
        /* 开机和关机分别选择协议库中的固定寄存器和值。 */
        if(request->type == INV_CONTROL_POWER_ON) {
            rt_memcpy(&default_reg, &protocol->ctrl.pwr_on, sizeof(default_reg));
            active->target = &g_inv_data[request->archive_index].ctrl.pwr_on;
            active->name = "power_on";
        }
        /* 关机请求选择关机固定值及对应的实时回读目标。 */
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
        /* 根据控制类型选择对应寄存器配置和实时数据回读目标。 */
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

/* 标记当前控制寄存器需要在普通周期抄读前优先读取一次。 */
static void inv_control_mark_refresh(Inv_Data_Port_Context_t *context)
{
    uint8_t archive_index = context->active.control.request.archive_index; /* 控制请求所属档案。 */
    uint8_t control_type = (uint8_t)context->active.control.request.type;   /* 需要优先回读的控制类型。 */
    uint8_t refresh_bit;                                                    /* 当前控制类型在位图中的标志位。 */

    /* 当前活动控制信息经过组帧检查，档案下标和控制类型应当都位于有效范围。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) || (control_type >= INV_CONTROL_TYPE_MAX)) {
        return;
    }

    refresh_bit = (uint8_t)(1U << control_type); /* 将控制类型转换为单个回读标志位。 */
    context->control_refresh_mask[archive_index] |= refresh_bit;
    context->control_refresh_pending = RT_TRUE;
    context->active.control.target->valid = 0U; /* 控制值可能已经改变，回读成功前旧实时值不再可信。 */
    rt_kprintf("%s uart[%d] archive[%d] control[%s] queued for priority read\n", get_char_time(), inv_data_uart_no(context), archive_index + 1, context->active.control.name);
}

/* 从当前端口位图中取出一项待回读控制寄存器，并准备活动控制配置。 */
static rt_bool_t inv_control_prepare_refresh(Inv_Data_Port_Context_t *context)
{
    Inv_Control_Request_t request; /* 用于复用控制配置查询接口的临时请求。 */
    Inv_Control_Result_t result;   /* 当前控制类型的协议配置查询结果。 */
    uint8_t archive_index;         /* 回读位图的档案遍历下标。 */
    uint8_t control_type;          /* 单档案内的控制类型遍历下标。 */
    uint8_t refresh_bit;           /* 当前控制类型对应的位图掩码。 */

    /* 没有任何回读标志时直接返回，避免每次周期调度扫描全部档案和控制类型。 */
    if(context->control_refresh_pending != RT_TRUE) {
        return RT_FALSE;
    }

    /* 每个档案的每种控制类型最多保留一个回读标志，相同控制连续写入会自动合并。 */
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        /* 在当前档案内依次检查七种可能待回读的控制类型。 */
        for(control_type = 0U; control_type < INV_CONTROL_TYPE_MAX; ++control_type) {
            refresh_bit = (uint8_t)(1U << control_type);

            /* 当前类型没有置位时跳过，不生成无意义回读请求。 */
            if((context->control_refresh_mask[archive_index] & refresh_bit) == 0U) {
                continue;
            }

            /* 选中后立即清除标志，组帧、发送、响应或超时无论结果如何都只尝试读取一次。 */
            context->control_refresh_mask[archive_index] &= (uint8_t)(~refresh_bit);

            /* 档案失效或接入端口改变后不再读取原控制寄存器。 */
            if((g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) ||
               (g_inv_archive_lib.archives[archive_index].port != inv_data_archive_port(context))) {
                continue;
            }

            rt_memset(&request, 0, sizeof(request)); /* 清除临时请求中与配置查询无关的字段。 */
            request.archive_index = archive_index;
            request.type = (Inv_Control_Type_t)control_type;
            result = inv_control_get_active(&request, &context->active.control);

            /* 协议或控制配置已经变化时跳过本次回读，并继续寻找下一项待处理标志。 */
            if(result != INV_CONTROL_RESULT_OK) {
                rt_kprintf("%s uart[%d] archive[%d] control type[%d] priority read skipped, result[%d]\n", get_char_time(), inv_data_uart_no(context), archive_index + 1, control_type, result);
                continue;
            }

            context->active_archive_index = archive_index;
            context->slave_addr = g_inv_archive_lib.archives[archive_index].mb_addr;
            return RT_TRUE;
        }
    }

    context->control_refresh_pending = RT_FALSE; /* 全部位图均已清空，后续调度无需继续扫描。 */
    return RT_FALSE;
}

/* 按0～30数据点下标取得协议寄存器配置和对应实时数据存储位置。 */
static rt_bool_t inv_data_get_point(uint8_t archive_index,
                                    uint8_t point_index,
                                    Inv_Data_Point_Config_t *point)
{
    const Inv_Proto_t *protocol; /* 当前档案绑定的协议库。 */
    Inv_Data_t *data;            /* 当前档案对应的实时数据结构。 */
    uint8_t offset;              /* 数组类数据点在相应数组中的偏移。 */

    /* 档案下标、数据点下标或输出指针无效时无法取得抄读点。 */
    if((archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (point_index >= INV_DATA_POINT_COUNT) || (point == RT_NULL)) {
        return RT_FALSE;
    }

    protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得数据点寄存器配置来源。 */

    /* 档案没有匹配协议时不能取得寄存器配置。 */
    if(protocol == RT_NULL) {
        return RT_FALSE;
    }

    data = &g_inv_data[archive_index];     /* 实时数据槽位与档案下标一一对应。 */
    rt_memset(point, 0, sizeof(*point));   /* 清除未使用目标指针及名称等字段。 */

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
        /* 参数类点下标固定映射到协议库中的六项参数配置。 */
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
        /* 控制类点下标固定映射到协议库中的七项控制配置。 */
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
    context->idle_tick = rt_tick_get(); /* 记录10秒设备空闲周期的起点。 */
    context->active.read.idle_after_active = RT_FALSE;
    context->state = INV_DATA_PORT_DEVICE_IDLE;
//    rt_kprintf("%s uart[%d] archive[%d] all data read, wait 10s before next device\n", get_char_time(), inv_data_uart_no(context), archive_index + 1);
}

/* 从当前游标开始查找本端口下一项有效寄存器，并返回首个数据点下标供三相合并使用。 */
static rt_bool_t inv_data_find_next_point(Inv_Data_Port_Context_t *context, uint8_t *first_point_index)
{
    uint16_t attempt; /* 为避免无可读点时死循环而设置的有限查找次数。 */

    /* 输出指针为空时无法返回本次请求的首个数据点下标。 */
    if(first_point_index == RT_NULL) {
        return RT_FALSE;
    }

    /* 最多检查12个档案的全部31个点，避免没有可读点时形成死循环。 */
    for(attempt = 0U; attempt < (INVERTER_ARCHIVE_MAX_COUNT * INV_DATA_POINT_COUNT); ++attempt) {
        uint8_t archive_index = context->archive_index; /* 本次尝试对应的档案游标。 */
        uint8_t point_index = context->point_index;     /* 本次尝试对应的数据点游标。 */
        const Inv_Archive_t *archive = &g_inv_archive_lib.archives[archive_index]; /* 当前档案只读信息。 */
        Inv_Data_Point_Config_t point;                  /* 当前候选寄存器的统一配置。 */
        rt_bool_t archive_last_point = (point_index == (INV_DATA_POINT_COUNT - 1U)) ? RT_TRUE : RT_FALSE; /* 是否为档案末点。 */

        inv_data_advance_cursor(context); /* 先移动下一游标，当前点完成后可以直接继续查找。 */

        /* 无效档案或接入其他端口的档案不属于当前端口状态机。 */
        if((g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) ||
           (archive->port != inv_data_archive_port(context))) {
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
        rt_memset(&context->active.read, 0, sizeof(context->active.read)); /* 清除上一读事务的合并点信息。 */
        context->active.read.points[0] = point;
        context->active.read.point_count = 1U;
        context->active.read.reg_count = point.reg_count;
        context->active.read.idle_after_active = archive_last_point;
        *first_point_index = point_index; /* 首点下标只参与本次组帧，不需要长期保存在端口上下文。 */
        return RT_TRUE;
    }

    return RT_FALSE;
}

/* 按合并下标取得当前请求中的数据点配置，下标0对应第一个数据点。 */
static Inv_Data_Point_Config_t *inv_data_get_active_point(Inv_Data_Port_Context_t *context, uint8_t point_index)
{
    return &context->active.read.points[point_index];
}

/* 将地址连续且功能码相同的三相电压或三相电流合并到当前Modbus读请求。 */
static void inv_data_combine_phase_points(Inv_Data_Port_Context_t *context, uint8_t first_point_index)
{
    Inv_Data_Point_Config_t next_point;     /* 等待判断是否可合并的后一相数据点。 */
    Inv_Data_Point_Config_t *previous_point; /* 当前合并请求中的最后一个数据点。 */
    uint8_t group_last_index;                /* 当前三相电压或电流组的末点下标。 */
    uint8_t next_point_index;                /* 下一候选三相数据点下标。 */

    /* 数据点0～2是三相电压，数据点3～5是三相电流，其他类型不进行合并。 */
    if(first_point_index < ENUM_PHASE_MAX) {
        group_last_index = ENUM_PHASE_MAX - 1U;
    }
    /* 首点位于3～5时只允许继续合并同组三相电流。 */
    else if(first_point_index < (ENUM_PHASE_MAX * 2U)) {
        group_last_index = ENUM_PHASE_MAX * 2U - 1U;
    }
    /* 有功、无功、功率因数和参数控制点不使用本连续合并功能。 */
    else {
        return;
    }

    /* 最多合并同一组中的三个数据点，遇到地址不连续或配置不同立即停止。 */
    while(context->active.read.point_count < INV_DATA_PHASE_COMBINE_MAX) {
        next_point_index = first_point_index + context->active.read.point_count;

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

        previous_point = inv_data_get_active_point(context, context->active.read.point_count - 1U);

        /* 只有功能码相同并且后一个起始地址紧跟前一个寄存器结束地址时才能合并。 */
        if((next_point.function_code != context->active.read.points[0].function_code) ||
           (next_point.reg_addr != (previous_point->reg_addr + previous_point->reg_count))) {
            break;
        }

        /* 合并后的寄存器总数不能超过Modbus单次读寄存器数量上限。 */
        if((context->active.read.reg_count + next_point.reg_count) > MODBUS_READ_REG_MAX) {
            break;
        }

        context->active.read.points[context->active.read.point_count] = next_point;
        ++context->active.read.point_count;
        context->active.read.reg_count += next_point.reg_count;
        inv_data_advance_cursor(context); /* 后一个数据点已并入当前请求，游标同步移动到再下一点。 */
    }

    /* 实际合并成功时打印批量读取范围，普通单点读取不增加额外日志。 */
    // if(context->active.read.point_count > 1U) {
    //     rt_kprintf("%s uart[%d] archive[%d] combined read starts at[%s], values[%d], registers[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name, context->active.read.point_count, context->active.read.reg_count);
    // }
}

/* 当前请求失败或超时时清除请求内全部目标有效标志，保留旧值供故障分析。 */
static void inv_data_invalidate_active_point(Inv_Data_Port_Context_t *context)
{
    uint8_t point_index; /* 当前合并请求内的数据点遍历下标。 */

    /* 合并请求中的任意一点不能独立确认有效，因此失败时统一清除有效标志。 */
    for(point_index = 0U; point_index < context->active.read.point_count; ++point_index) {
        Inv_Data_Point_Config_t *point = inv_data_get_active_point(context, point_index); /* 当前待清除有效标志的数据点。 */

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
    /* 当前点是该逆变器最后一项时，完成后进入该端口独立的10秒空闲。 */
    if(context->active.read.idle_after_active == RT_TRUE) {
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
    uint8_t source[MODBUS_RTU_ADU_MAX];          /* 按Modbus高字节在前展开的临时字节序列。 */
    uint16_t byte_count = register_count * 2U;   /* 输入寄存器对应的总字节数量。 */
    uint16_t index;                              /* 字节转换循环下标。 */

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
        uint16_t register_index = index / 2U; /* 当前字节所属的输入寄存器下标。 */

        /* 偶数下标取寄存器高字节，奇数下标取寄存器低字节。 */
        if((index & 1U) == 0U) {
            source[index] = (uint8_t)(registers[register_index] >> 8U);
        }
        /* 多寄存器CDAB保持寄存器内部顺序并反转寄存器先后顺序。 */
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
        /* 多个寄存器时，CDAB保持寄存器内部字节不变并反转寄存器顺序。 */
        else {
            for(index = 0U; index + 1U < byte_count; index += 2U) {
                uint16_t source_index = byte_count - 2U - index; /* 反转后对应的源寄存器首字节。 */
                output[index] = source[source_index];
                output[index + 1U] = source[source_index + 1U];
            }
        }
    }
    /* NORMAL及未知字节序都保持Modbus线上字节顺序。 */
    /* NORMAL及未知字节序保持Modbus响应原始高字节在前顺序。 */
    else {
        rt_memcpy(output, source, byte_count);
    }

    return byte_count;
}

/* 计算10的decimal_places次方，供浮点数据转换为int32_t定点值。 */
static int32_t inv_data_decimal_scale(uint8_t decimal_places)
{
    int32_t scale = 1; /* 最终返回的10次幂缩放因子。 */
    uint8_t index;     /* 已完成乘10的小数位计数。 */

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
    uint8_t logical_bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0}; /* 高有效字节在前的逻辑数值字节。 */
    uint8_t wire_bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0};    /* 按协议字节序排列的线上字节。 */
    uint64_t raw_value = 0U;                                /* 整数或浮点位模式的统一容器。 */
    uint64_t raw_float64;                                   /* float64控制值的原始位模式。 */
    uint32_t raw_float32;                                   /* float32控制值的原始位模式。 */
    float float32_value;                                    /* 按小数位还原的单精度控制值。 */
    double float64_value;                                   /* 按小数位还原的双精度控制值。 */
    uint16_t byte_count = active->reg_count * 2U;            /* 控制寄存器对应的有效字节数。 */
    uint16_t index;                                         /* 字节或寄存器转换循环下标。 */

    /* 当前控制接口不允许写入超过内部数值缓冲区容量的寄存器数据。 */
    if((active->reg_count == 0U) || (byte_count > INV_DATA_NUMERIC_BYTE_MAX)) {
        return RT_FALSE;
    }

    /* 根据协议数据类型检查数值范围并取得需要写入的原始位模式。 */
    switch(active->data_type) { /* 每个分支把调用方int32_t定点值转换为协议原始位模式。 */
    case TYPE_I8:
        /* 有符号8位控制值必须处于INT8范围。 */
        if((active->request.value < INT8_MIN) || (active->request.value > INT8_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint8_t)(int8_t)active->request.value;
        break;

    case TYPE_U8:
        /* 无符号8位控制值不允许为负数或超过UINT8上限。 */
        if((active->request.value < 0) || (active->request.value > UINT8_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint8_t)active->request.value;
        break;

    case TYPE_I16:
        /* 有符号16位控制值必须处于INT16范围。 */
        if((active->request.value < INT16_MIN) || (active->request.value > INT16_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint16_t)(int16_t)active->request.value;
        break;

    case TYPE_U16:
        /* 无符号16位控制值不允许为负数或超过UINT16上限。 */
        if((active->request.value < 0) || (active->request.value > UINT16_MAX)) {
            return RT_FALSE;
        }
        raw_value = (uint16_t)active->request.value;
        break;

    case TYPE_I32:
        /* 32位数据至少需要两个16位寄存器承载。 */
        if(byte_count < 4U) {
            return RT_FALSE;
        }
        raw_value = (uint32_t)active->request.value;
        break;

    case TYPE_U32:
    case TYPE_BIT_FIELD:
    case TYPE_BCD_TIME:
        /* 无符号32位及位域类型要求非负值并至少配置两个寄存器。 */
        if((active->request.value < 0) || (byte_count < 4U)) {
            return RT_FALSE;
        }
        raw_value = (uint32_t)active->request.value;
        break;

    case TYPE_FLOAT32:
        /* 单精度浮点位模式至少需要两个寄存器。 */
        if(byte_count < 4U) {
            return RT_FALSE;
        }
        float32_value = (float)active->request.value / inv_data_decimal_scale(active->decimal_places);
        rt_memcpy(&raw_float32, &float32_value, sizeof(raw_float32));
        raw_value = raw_float32;
        break;

    case TYPE_FLOAT64:
        /* 双精度浮点位模式必须完整占用四个寄存器。 */
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
        /* 每次交换一个寄存器对应的两个字节。 */
        for(index = 0U; index + 1U < byte_count; index += 2U) {
            wire_bytes[index] = logical_bytes[index + 1U];
            wire_bytes[index + 1U] = logical_bytes[index];
        }
    }
    /* DCBA将全部数据字节完全反序。 */
    else if(active->byte_order == Type_Byte_DCBA) {
        /* 按源数组末尾到开头的顺序逐字节复制。 */
        for(index = 0U; index < byte_count; ++index) {
            wire_bytes[index] = logical_bytes[byte_count - 1U - index];
        }
    }
    /* CDAB对单寄存器交换字节，对多寄存器反转寄存器顺序。 */
    else if(active->byte_order == Type_Byte_CDAB) {
        /* 单寄存器和多寄存器的CDAB含义不同，需要分别处理。 */
        if(byte_count == 2U) {
            wire_bytes[0] = logical_bytes[1];
            wire_bytes[1] = logical_bytes[0];
        }
        /* 多寄存器CDAB反转寄存器顺序但保持每个寄存器内部字节顺序。 */
        else {
            /* 每次复制一个反转位置上的完整16位寄存器。 */
            for(index = 0U; index + 1U < byte_count; index += 2U) {
                uint16_t source_index = byte_count - 2U - index; /* 反转后的源寄存器首字节。 */
                wire_bytes[index] = logical_bytes[source_index];
                wire_bytes[index + 1U] = logical_bytes[source_index + 1U];
            }
        }
    }
    /* NORMAL保持最高有效字节在前，未知字节序配置直接判定为转换失败。 */
    else if(active->byte_order == Type_Byte_ABCD) {
        rt_memcpy(wire_bytes, logical_bytes, byte_count);
    }
    /* 未知字节序无法可靠生成控制报文，直接返回转换失败。 */
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
    int32_t result = 0; /* 逐字节累加得到的十进制整数。 */
    uint16_t index;     /* BCD输入字节遍历下标。 */

    /* 输入或输出指针为空时不能解析BCD。 */
    if((bytes == RT_NULL) || (value == RT_NULL)) {
        return RT_FALSE;
    }

    /* 每个字节按高位十进制数字、低位十进制数字依次累加。 */
    for(index = 0U; index < byte_count; ++index) {
        uint8_t high = bytes[index] >> 4U;   /* 当前字节高半字节表示的十进制数字。 */
        uint8_t low = bytes[index] & 0x0FU;  /* 当前字节低半字节表示的十进制数字。 */

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
    uint8_t bytes[INV_DATA_NUMERIC_BYTE_MAX] = {0}; /* 按协议字节序恢复的连续数值字节。 */
    uint64_t raw_value = 0U;                        /* 连续字节组合后的无符号原始位模式。 */
    uint32_t raw_float;                             /* float32数据的32位原始位模式。 */
    float float32_value;                            /* 从原始位模式恢复的单精度值。 */
    double float64_value;                           /* 从原始位模式恢复的双精度值。 */
    double scaled_value;                            /* 浮点值按协议小数位放大后的定点值。 */
    uint16_t byte_count;                            /* 实际转换得到的有效字节数量。 */
    uint16_t index;                                 /* 原始字节组合循环下标。 */

    byte_count = inv_data_registers_to_bytes(registers, point->reg_count, point->byte_order, bytes, sizeof(bytes)); /* 先还原协议逻辑字节序。 */

    /* 没有得到任何数据字节时不能生成实时值。 */
    if(byte_count == 0U) {
        return RT_FALSE;
    }

    /* 连续字节按高字节在前组合为无符号原始值。 */
    for(index = 0U; index < byte_count; ++index) {
        raw_value = (raw_value << 8U) | bytes[index];
    }

    /* 不同协议数据类型最终统一转换为int32_t保存。 */
    switch(point->data_type) { /* 按协议类型解释原始位模式并统一输出int32_t。 */
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
    Inv_RealtimeString_t *target = point->string_target; /* 设备编号的实时字符串存储位置。 */
    uint8_t bytes[INV_DATA_DEVICE_NO_MAX_LEN];            /* 按协议字节序恢复的设备编号字节。 */
    uint16_t byte_count;                                  /* 实际恢复的设备编号字节数。 */
    uint16_t index;                                       /* 字符串复制循环下标。 */

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
        int32_t number; /* 非ASCII设备编号解析得到的整数。 */

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
    int32_t value; /* 当前寄存器按协议类型解析得到的统一实时整数。 */

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
    uint16_t register_offset = 0U; /* 当前数据点在合并响应寄存器数组中的偏移。 */
    uint8_t point_index;           /* 合并响应中的数据点遍历下标。 */

    /* 普通请求循环一次，三相连续请求最多循环三次。 */
    for(point_index = 0U; point_index < context->active.read.point_count; ++point_index) {
        Inv_Data_Point_Config_t *point = inv_data_get_active_point(context, point_index); /* 当前待保存的合并数据点配置。 */

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
    uint8_t frame[MODBUS_READ_REQUEST_LEN]; /* 标准Modbus读请求报文缓冲区。 */
    uint8_t first_point_index;              /* 当前请求首点下标，用于判断三相连续合并。 */
    uint16_t frame_len = 0U;                /* 主机协议接口返回的请求报文长度。 */
    rt_size_t written_size;                 /* 串口管理接口实际接受的发送字节数。 */
//    char frame_name[96];

    /* 当前端口没有有效档案或可读寄存器时保持READY，等待下一次调度重新检查。 */
    if(inv_data_find_next_point(context, &first_point_index) == RT_FALSE) {
        return;
    }

    inv_data_combine_phase_points(context, first_point_index); /* 首点下标仅在本次组帧阶段用于连续三相数据判断。 */

    /* 当前寄存器配置无法组成合法Modbus请求时将该数据点置为无效。 */
    if(modbus_m_read_request(context->slave_addr,
                             context->active.read.points[0].function_code,
                             context->active.read.points[0].reg_addr,
                             context->active.read.reg_count,
                             frame,
                             sizeof(frame),
                             &frame_len) != RT_EOK) {
        rt_kprintf("%s uart[%d] archive[%d] could not build request for data[%s]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name);
        inv_data_invalidate_active_point(context);
        inv_data_finish_active_point(context);
        return;
    }

    // rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] addr[%d] data[%s] values[%d] request", inv_data_uart_no(context), context->active_archive_index + 1, context->slave_addr, context->active.read.points[0].name, context->active.read.point_count);
    // show_arr(frame_name, frame, frame_len);
    written_size = uart_mgmt_write(inv_data_uart_no(context), frame, frame_len); /* 非阻塞提交完整Modbus请求到目标串口。 */

    /* 请求没有完整写入串口时不启动响应超时，下一次调度继续读取后续数据点。 */
    if(written_size != frame_len) {
        rt_kprintf("%s uart[%d] archive[%d] could not send full data[%s] request, expected[%d], sent[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name, frame_len, written_size);
        inv_data_invalidate_active_point(context);
        inv_data_finish_active_point(context);
        return;
    }

    context->request_tick = rt_tick_get();
    context->state = INV_DATA_PORT_WAIT_PERIODIC_READ;
}

/* 从端口接收邮箱安全取出一帧，复制完成后立即释放邮箱。 */
static uint16_t inv_data_take_rx_frame(Inv_Data_Port_Context_t *context, uint8_t *frame)
{
    rt_base_t level;    /* 复制单帧邮箱前保存的中断级别。 */
    uint16_t frame_len; /* 从邮箱取出的完整响应长度。 */

    level = rt_hw_interrupt_disable(); /* 防止串口接收回调在复制过程中覆盖邮箱。 */
    frame_len = context->rx_frame_len;
    rt_memcpy(frame, context->rx_frame, frame_len);
    context->rx_ready = RT_FALSE;
    rt_hw_interrupt_enable(level);
    return frame_len;
}

/* 解析当前端口收到的响应，成功时更新实时数据，失败时立即进入下一数据点。 */
static void inv_data_handle_response(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[INV_DATA_RX_FRAME_SIZE];       /* 从端口邮箱复制出的响应报文。 */
    uint16_t registers[MODBUS_READ_REG_MAX];     /* 解析成功后保存的寄存器数组。 */
    uint16_t frame_len;                          /* 当前响应报文的有效长度。 */
    uint16_t register_count = 0U;                /* 主机解析接口返回的寄存器数量。 */
    uint8_t exception_code = 0U;                 /* 从站异常响应中的Modbus异常码。 */
    modbus_m_parse_result result;                 /* 当前周期读响应的完整校验结果。 */
    char frame_name[112];                        /* 调试数组打印使用的报文名称缓冲区。 */

    frame_len = inv_data_take_rx_frame(context, frame); /* 取走报文后立即释放端口单帧邮箱。 */
    result = modbus_m_read_response(context->slave_addr,
                                    context->active.read.points[0].function_code,
                                    context->active.read.reg_count,
                                    frame,
                                    frame_len,
                                    registers,
                                    MODBUS_READ_REG_MAX,
                                    &register_count,
                                    &exception_code);
//    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] data[%s] values[%d] reply[%s]", inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name, context->active.read.point_count, modbus_m_parse_result_text(result));
//    show_arr(frame_name, frame, frame_len);

    /* 报文解析成功并且实时数据转换成功时打印本次保存结果。 */
    if((result == MODBUS_M_PARSE_OK) &&
       (register_count == context->active.read.reg_count) &&
       (inv_data_store_active_values(context, registers) == RT_TRUE)) {
//        rt_kprintf("%s uart[%d] archive[%d] saved data[%s], values[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name, context->active.read.point_count);
    }
     /* 收到报文但解析或数据转换失败时，本次请求立即结束，不再继续计算超时。 */
     else {
         rt_kprintf("%s uart[%d] archive[%d] data[%s] reply rejected: %s, exception[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.read.points[0].name, modbus_m_parse_result_text(result), exception_code);
         inv_data_invalidate_active_point(context);
     }

    inv_data_finish_active_point(context);
}

/* 控制事务结束后恢复写请求插入前的周期抄读状态。 */
static void inv_control_finish(Inv_Data_Port_Context_t *context)
{
    context->state = context->resume_state;
}

/* 从当前端口队列取出一项控制请求，转换控制值并发送Modbus写报文。 */
static void inv_control_send_next_request(Inv_Data_Port_Context_t *context)
{
    Inv_Control_Request_t request;            /* 从当前端口队列取出的最早控制请求。 */
    Inv_Control_Result_t result;              /* 请求配置检查或转换阶段的控制结果。 */
    uint8_t frame[MODBUS_RTU_ADU_MAX];        /* 06或10功能码写请求报文缓冲区。 */
    uint16_t frame_len = 0U;                  /* 主机协议接口返回的写请求长度。 */
    rt_size_t written_size;                   /* 串口管理接口实际接受的发送字节数。 */
    char frame_name[112];                     /* show_arr打印控制请求时使用的名称。 */

    /* 队列可能在状态判断后被其他线程改变，实际取不到请求时直接返回。 */
    if(inv_control_queue_get(&context->control_queue, &request) == RT_FALSE) {
        return;
    }

    result = inv_control_get_active(&request, &context->active.control); /* 把控制类型转换为当前协议寄存器配置。 */

    /* 档案或协议控制配置无效时生成结果，不占用串口等待状态。 */
    if(result != INV_CONTROL_RESULT_OK) {
        inv_control_push_result(&request, result, 0U);
        rt_kprintf("%s uart[%d] archive[%d] control type[%d] cannot run, result[%d]\n", get_char_time(), inv_data_uart_no(context), request.archive_index + 1, request.type, result);
        return;
    }

    context->active_archive_index = request.archive_index;
    context->slave_addr = g_inv_archive_lib.archives[request.archive_index].mb_addr;

    /* 控制值必须先按照协议数据类型和字节序转换为线上寄存器数据。 */
    if(inv_control_value_to_registers(&context->active.control) == RT_FALSE) {
        inv_control_push_result(&context->active.control.request, INV_CONTROL_RESULT_BUILD_FAILED, 0U);
        rt_kprintf("%s uart[%d] archive[%d] control[%s] value[%d] cannot be converted\n", get_char_time(), inv_data_uart_no(context), request.archive_index + 1, context->active.control.name, context->active.control.request.value);
        return;
    }

    /* 根据协议库配置的06或10功能码组成实际下行控制请求。 */
    if(modbus_m_write_request(context->slave_addr,
                              context->active.control.function_code,
                              context->active.control.reg_addr,
                              context->active.control.registers,
                              context->active.control.reg_count,
                              frame,
                              sizeof(frame),
                              &frame_len) != RT_EOK) {
        inv_control_push_result(&context->active.control.request, INV_CONTROL_RESULT_BUILD_FAILED, 0U);
        rt_kprintf("%s uart[%d] archive[%d] could not build control[%s] request\n", get_char_time(), inv_data_uart_no(context), request.archive_index + 1, context->active.control.name);
        return;
    }

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] addr[%d] control[%s] request", inv_data_uart_no(context), request.archive_index + 1, context->slave_addr, context->active.control.name);
    show_arr(frame_name, frame, frame_len); /* 下发前打印完整控制报文，便于核对寄存器和值。 */
    written_size = uart_mgmt_write(inv_data_uart_no(context), frame, frame_len); /* 将写请求提交到对应物理串口。 */

    /* 控制请求没有完整写入串口时立即生成失败结果，不启动1秒响应超时。 */
    if(written_size != frame_len) {
        inv_control_push_result(&context->active.control.request, INV_CONTROL_RESULT_SEND_FAILED, 0U);
        rt_kprintf("%s uart[%d] archive[%d] could not send full control[%s] request, expected[%d], sent[%d]\n", get_char_time(), inv_data_uart_no(context), request.archive_index + 1, context->active.control.name, frame_len, written_size);
        return;
    }

    /* 写请求发送成功后保存原状态，写事务结束再继续原来的周期抄读或10秒空闲。 */
    context->resume_state = context->state;
    context->request_tick = rt_tick_get();
    context->state = INV_DATA_PORT_WAIT_CONTROL_WRITE;
}

/* 在普通周期抄读前发送一项待更新控制寄存器的03功能码读请求。 */
static rt_bool_t inv_control_send_refresh_request(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[MODBUS_READ_REQUEST_LEN]; /* 控制寄存器03功能码回读请求。 */
    uint16_t frame_len = 0U;                /* 生成的优先回读报文长度。 */
    rt_size_t written_size;                 /* 串口实际接受的优先回读字节数。 */
    char frame_name[112];                   /* show_arr打印回读请求时使用的名称。 */

    /* 没有待回读标志时返回RT_FALSE，调度器可以继续执行普通周期抄读。 */
    if(inv_control_prepare_refresh(context) == RT_FALSE) {
        return RT_FALSE;
    }

    /* 控制寄存器优先回读固定使用03功能码，并沿用写控制的地址和寄存器数量。 */
    if(modbus_m_read_request(context->slave_addr,
                             MODBUS_FUNC_READ_HOLDING,
                             context->active.control.reg_addr,
                             context->active.control.reg_count,
                             frame,
                             sizeof(frame),
                             &frame_len) != RT_EOK) {
        context->active.control.target->valid = 0U;
        rt_kprintf("%s uart[%d] archive[%d] could not build control[%s] priority read request\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name);
        return RT_TRUE;
    }

    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] addr[%d] control[%s] priority read request", inv_data_uart_no(context), context->active_archive_index + 1, context->slave_addr, context->active.control.name);
    show_arr(frame_name, frame, frame_len); /* 打印控制寄存器优先回读请求。 */
    written_size = uart_mgmt_write(inv_data_uart_no(context), frame, frame_len);

    /* 回读请求没有完整写入串口时本次尝试结束，下一次调度继续处理其他任务。 */
    if(written_size != frame_len) {
        context->active.control.target->valid = 0U;
        rt_kprintf("%s uart[%d] archive[%d] could not send full control[%s] priority read, expected[%d], sent[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, frame_len, written_size);
        return RT_TRUE;
    }

    /* 保存进入回读前的READY或设备空闲状态，回读结束后继续原来的周期流程。 */
    context->resume_state = context->state;
    context->request_tick = rt_tick_get();
    context->state = INV_DATA_PORT_WAIT_CONTROL_REFRESH;
    return RT_TRUE;
}

/* 解析当前控制写响应，生成异步结果并恢复该端口原来的周期状态。 */
static void inv_control_handle_response(Inv_Data_Port_Context_t *context)
{
    uint8_t frame[INV_DATA_RX_FRAME_SIZE]; /* 从邮箱取出的控制写响应。 */
    uint16_t frame_len;                    /* 控制写响应的有效长度。 */
    uint8_t exception_code = 0U;           /* 从站异常响应中的异常码。 */
    modbus_m_parse_result parse_result;     /* 写响应地址、功能码、数据和CRC校验结果。 */
    Inv_Control_Result_t control_result;    /* 转换给上层使用的异步控制结果。 */
    char frame_name[112];                   /* show_arr打印控制响应时使用的名称。 */

    frame_len = inv_data_take_rx_frame(context, frame);
    parse_result = modbus_m_write_response(context->slave_addr,
                                           context->active.control.function_code,
                                           context->active.control.reg_addr,
                                           context->active.control.registers,
                                           context->active.control.reg_count,
                                           frame,
                                           frame_len,
                                           &exception_code);
    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] control[%s] reply[%s]", inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, modbus_m_parse_result_text(parse_result));
    show_arr(frame_name, frame, frame_len); /* 收到报文后统一打印原始内容和解析结果。 */

    /* 完整写响应校验通过时只确认控制成功，实时值留给后续优先回读更新。 */
    if(parse_result == MODBUS_M_PARSE_OK) {
        control_result = INV_CONTROL_RESULT_OK;
        inv_control_mark_refresh(context);
        rt_kprintf("%s uart[%d] archive[%d] control[%s] write finished, value[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, context->active.control.request.value);
    }
    /* 从站异常响应需要保留异常码，便于上层转换成自己的应答状态。 */
    else if(parse_result == MODBUS_M_PARSE_EXCEPTION) {
        control_result = INV_CONTROL_RESULT_DEVICE_EXCEPTION;
        rt_kprintf("%s uart[%d] archive[%d] control[%s] device exception[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, exception_code);
    }
    /* 已收到但不能匹配本次写请求的报文直接结束，不再继续计算超时。 */
    else {
        control_result = INV_CONTROL_RESULT_RESPONSE_INVALID;
        inv_control_mark_refresh(context); /* 写请求可能已经执行，仅响应损坏时仍安排一次实际值回读。 */
        rt_kprintf("%s uart[%d] archive[%d] control[%s] reply rejected: %s\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, modbus_m_parse_result_text(parse_result));
    }

    inv_control_push_result(&context->active.control.request, control_result, exception_code);
    inv_control_finish(context);
}

/* 写请求1秒内没有收到任何报文时生成超时结果，并恢复原周期状态。 */
static void inv_control_handle_timeout(Inv_Data_Port_Context_t *context)
{
    rt_kprintf("%s uart[%d] archive[%d] addr[%d] control[%s] no reply within 1s\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->slave_addr, context->active.control.name);
    inv_control_mark_refresh(context); /* 响应丢失不能证明写入失败，周期流程仍优先读取一次实际值。 */
    inv_control_push_result(&context->active.control.request, INV_CONTROL_RESULT_TIMEOUT, 0U);
    inv_control_finish(context);
}

/* 解析控制寄存器优先回读响应，成功时使用实际寄存器值更新实时控制数据。 */
static void inv_control_handle_refresh_response(Inv_Data_Port_Context_t *context)
{
    Inv_Data_Point_Config_t point;                 /* 由活动控制配置临时构造的数值解析配置。 */
    uint8_t frame[INV_DATA_RX_FRAME_SIZE];         /* 从邮箱取出的控制寄存器读响应。 */
    uint16_t registers[INV_CONTROL_REGISTER_MAX];  /* 回读解析得到的控制寄存器数据。 */
    uint16_t frame_len;                            /* 优先回读响应的有效长度。 */
    uint16_t register_count = 0U;                  /* 实际解析出的控制寄存器数量。 */
    uint8_t exception_code = 0U;                   /* 回读异常响应中的Modbus异常码。 */
    int32_t value;                                 /* 按协议类型转换后的控制实际值。 */
    modbus_m_parse_result parse_result;             /* 优先回读响应的完整解析结果。 */
    char frame_name[112];                          /* show_arr打印回读响应时使用的名称。 */

    frame_len = inv_data_take_rx_frame(context, frame);
    parse_result = modbus_m_read_response(context->slave_addr,
                                          MODBUS_FUNC_READ_HOLDING,
                                          context->active.control.reg_count,
                                          frame,
                                          frame_len,
                                          registers,
                                          INV_CONTROL_REGISTER_MAX,
                                          &register_count,
                                          &exception_code);
    rt_snprintf(frame_name, sizeof(frame_name), "uart[%d] archive[%d] control[%s] priority read reply[%s]", inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, modbus_m_parse_result_text(parse_result));
    show_arr(frame_name, frame, frame_len);

    rt_memset(&point, 0, sizeof(point)); /* 只填写数值解析所需字段，其他字段保持为空。 */
    point.reg_count = context->active.control.reg_count;
    point.data_type = context->active.control.data_type;
    point.byte_order = context->active.control.byte_order;
    point.decimal_places = context->active.control.decimal_places;

    /* 报文、寄存器数量和数值转换全部有效时才用实际回读值更新实时数据。 */
    if((parse_result == MODBUS_M_PARSE_OK) &&
       (register_count == context->active.control.reg_count) &&
       (inv_data_decode_number(&point, registers, &value) == RT_TRUE)) {
        context->active.control.target->value = value;
        context->active.control.target->update_tick = rt_tick_get();
        context->active.control.target->valid = 1U;
        rt_kprintf("%s uart[%d] archive[%d] control[%s] priority read saved value[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, value);
    }
    /* 收到报文但解析失败时保持数据无效，本次优先回读不再重试。 */
    else {
        context->active.control.target->valid = 0U;
        rt_kprintf("%s uart[%d] archive[%d] control[%s] priority read rejected: %s, exception[%d]\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->active.control.name, modbus_m_parse_result_text(parse_result), exception_code);
    }

    inv_control_finish(context);
}

/* 控制寄存器优先回读1秒内没有收到报文时结束本次尝试。 */
static void inv_control_handle_refresh_timeout(Inv_Data_Port_Context_t *context)
{
    context->active.control.target->valid = 0U;
    rt_kprintf("%s uart[%d] archive[%d] addr[%d] control[%s] priority read no reply within 1s\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->slave_addr, context->active.control.name);
    inv_control_finish(context);
}

/* 每次调度只推进一个端口状态机的一步，三个端口之间不会互相等待。 */
static void inv_data_process_port(Inv_Data_Port_Context_t *context, rt_tick_t now)
{
    /* 等待响应时必须先结束当前线上事务，排队的实时控制不能破坏读写应答对应关系。 */
    if(inv_data_state_waiting_response(context->state) == RT_TRUE) {
        /* 收到完整响应时直接按等待状态解析，状态本身已经包含事务类型。 */
        if(context->rx_ready == RT_TRUE) {
            /* 等待控制写响应时按写回显规则解析并生成异步结果。 */
            if(context->state == INV_DATA_PORT_WAIT_CONTROL_WRITE) {
                inv_control_handle_response(context);
            }
            /* 等待优先回读响应时解析实际控制值并更新实时数据。 */
            else if(context->state == INV_DATA_PORT_WAIT_CONTROL_REFRESH) {
                inv_control_handle_refresh_response(context);
            }
            /* 其余等待状态只可能是普通周期抄读。 */
            else {
                inv_data_handle_response(context);
            }
            return;
        }

        /* 只有没有收到报文并且等待达到1秒时，才结束当前读写事务。 */
        if((rt_tick_t)(now - context->request_tick) >= INV_DATA_RESPONSE_TIMEOUT_TICKS) {
            /* 控制写无报文时生成超时结果并安排一次控制值回读。 */
            if(context->state == INV_DATA_PORT_WAIT_CONTROL_WRITE) {
                inv_control_handle_timeout(context);
            }
            /* 控制回读超时时保持实时值无效并恢复此前周期状态。 */
            else if(context->state == INV_DATA_PORT_WAIT_CONTROL_REFRESH) {
                inv_control_handle_refresh_timeout(context);
            }
            /* 普通周期读超时时使当前数据失效并继续后续数据点。 */
            else {
                rt_kprintf("%s uart[%d] archive[%d] addr[%d] data[%s] no reply within 1s\n", get_char_time(), inv_data_uart_no(context), context->active_archive_index + 1, context->slave_addr, context->active.read.points[0].name);
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

    /* 控制写流程结束后，周期调度在普通数据点和10秒空闲前优先回读控制寄存器一次。 */
    if(inv_control_send_refresh_request(context) == RT_TRUE) {
        return;
    }

    /* 非工作时段不再推进设备空闲或发送新周期请求，已发送及已受理控制已在前面完成处理。 */
    if(g_inv_data_work_enabled != RT_TRUE) {
        return;
    }

    /* 设备空闲状态达到10秒后恢复READY，下一次调度开始读取下一台逆变器。 */
    if(context->state == INV_DATA_PORT_DEVICE_IDLE) {
        /* 当前tick与空闲起点差值达到10秒时结束该设备空闲周期。 */
        if((rt_tick_t)(now - context->idle_tick) >= INV_DATA_DEVICE_IDLE_TICKS) {
            context->state = INV_DATA_PORT_READY;
//            rt_kprintf("%s uart[%d] 10s wait finished, continue reading\n", get_char_time(), inv_data_uart_no(context));
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
    rt_err_t semaphore_result; /* 静态控制结果信号量的初始化返回值。 */

    g_inv_data_initialized = RT_FALSE;
    g_inv_data_work_enabled = RT_FALSE;
    g_inv_data_work_state_initialized = RT_FALSE;
    rt_memset(g_inv_data, 0, sizeof(g_inv_data)); /* 上电时全部实时数据默认为无效。 */
    rt_memset(g_inv_data_ports, 0, sizeof(g_inv_data_ports)); /* 清除三个端口的游标、队列和接收邮箱。 */
    rt_memset(g_inv_control_results, 0, sizeof(g_inv_control_results)); /* 清除尚未被上层读取的旧结果。 */
    g_inv_control_result_read_index = 0U;
    g_inv_control_result_write_index = 0U;
    g_inv_control_result_count = 0U;

    /* 首次初始化时创建静态结果信号量，初始计数为0表示没有完成结果。 */
    if(g_inv_control_result_sem_initialized != RT_TRUE) {
        semaphore_result = rt_sem_init(&g_inv_control_result_sem, "ctrl_res", 0U, RT_IPC_FLAG_FIFO);
        /* 信号量创建失败时保持模块未初始化，禁止外部提交控制。 */
        if(semaphore_result != RT_EOK) {
            rt_kprintf("%s control result semaphore init failed, result[%d]\n", get_char_time(), semaphore_result);
            return;
        }
        g_inv_control_result_sem_initialized = RT_TRUE;
    }
    /* 重复初始化实时数据时清空旧信号，保证信号量计数与已清空的结果队列一致。 */
    else {
        /* 反复尝试非阻塞取信号，直到已有控制结果信号全部清空。 */
        while(rt_sem_trytake(&g_inv_control_result_sem) == RT_EOK) {
            /* 循环取走全部旧信号，直到信号量计数归零。 */
        }
    }

    g_inv_data_ports[0].port_index = 0U;
    g_inv_data_ports[1].port_index = 1U;
    g_inv_data_ports[2].port_index = 2U;
    g_inv_data_initialized = RT_TRUE;
}

/* 周期抄读主循环顺序推进三个独立状态机，不会串行等待某一路响应超时。 */
void Inv_Data_Poll_Loop(void)
{
    rt_tick_t time_check_tick; /* 上一次检查RTC工作窗口时记录的tick。 */
    uint8_t index;             /* 三个端口状态机的调度下标。 */

    Inv_Data_Init(); /* 在线程循环前初始化实时数据、端口上下文和控制结果同步对象。 */
    inv_data_update_work_state(); /* 线程启动时立即确定当前是否允许周期抄读和控制。 */
    time_check_tick = rt_tick_get();

    /* 周期抄读线程持续运行，档案变化后下一轮会自动重新检查有效槽位。 */
    while(1) {
        rt_tick_t now = rt_tick_get(); /* 本轮三个端口共用同一tick基准进行超时判断。 */

        /* 当前工程1tick等于1ms，每秒检查一次RTC即可及时处理7点和17点切换。 */
        if((rt_tick_t)(now - time_check_tick) >= INV_DATA_TIME_CHECK_TICKS) {
            time_check_tick = now;
            inv_data_update_work_state(); /* 处理07:00启动和17:00停止边界及对应日志。 */
        }

        /* 每次调度分别推进三个端口，不在某个端口内部阻塞等待响应。 */
        for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
            inv_data_process_port(&g_inv_data_ports[index], now); /* 每个端口每轮只推进一个状态步骤。 */
        }

        rt_thread_mdelay(INV_DATA_POLL_TICKS); /* 释放CPU并维持约10ms的状态机调度周期。 */
    }
}

/* 串口管理层提交周期读取或控制写响应，每个端口使用自己的单帧接收邮箱。 */
rt_err_t Inv_Data_Rx_Frame(uint16_t uart_no, const uint8_t *frame, uint16_t frame_len)
{
    Inv_Data_Port_Context_t *context = RT_NULL; /* 与接收串口对应的端口状态机。 */
    rt_base_t level;                            /* 写单帧邮箱前保存的中断级别。 */
    uint8_t index;                              /* 三个端口配置的查找下标。 */

    /* 报文指针为空、长度为0或超过接收邮箱容量时拒绝接收。 */
    if((frame == RT_NULL) || (frame_len == 0U) || (frame_len > INV_DATA_RX_FRAME_SIZE)) {
        return -RT_EINVAL;
    }

    /* 按串口编号查找对应的下行读写端口上下文。 */
    for(index = 0U; index < INV_DATA_PORT_COUNT; ++index) {
        /* 串口编号匹配时保存对应上下文并结束查找。 */
        if(inv_data_uart_no(&g_inv_data_ports[index]) == uart_no) {
            context = &g_inv_data_ports[index];
            break;
        }
    }

    /* 非下行管理串口或当前端口没有等待读写响应时不接收报文。 */
    if((context == RT_NULL) || (inv_data_state_waiting_response(context->state) != RT_TRUE)) {
        return -RT_EBUSY;
    }

    level = rt_hw_interrupt_disable(); /* 防止周期线程同时取走正在写入的报文。 */

    /* 上一帧尚未处理时禁止覆盖该端口的单帧邮箱。 */
    if(context->rx_ready == RT_TRUE) {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;
    }

    rt_memcpy(context->rx_frame, frame, frame_len); /* 完整复制后再置rx_ready，避免读取半帧。 */
    context->rx_frame_len = frame_len;
    context->rx_ready = RT_TRUE;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/* 异步提交一项逆变器控制请求，同一物理端口按照提交顺序依次执行。 */
rt_err_t Inv_Control_Submit(const Inv_Control_Request_t *request)
{
    Inv_Data_Port_Context_t *context; /* 目标档案所在线路的独立端口状态机。 */
    rt_base_t level;                  /* 写控制请求队列前保存的中断级别。 */

    /* 周期线程尚未初始化、请求为空或参数越界时不能进入控制队列。 */
    if(g_inv_data_initialized != RT_TRUE) {
        return -RT_EBUSY;
    }

    /* 请求地址、档案下标和控制类型必须全部位于公开接口允许范围。 */
    if((request == RT_NULL) || (request->archive_index >= INVERTER_ARCHIVE_MAX_COUNT) ||
       (request->type < INV_CONTROL_POWER_ON) || (request->type >= INV_CONTROL_TYPE_MAX)) {
        return -RT_EINVAL;
    }

    context = inv_control_find_port_context(request->archive_index); /* 按档案端口把请求路由到对应串口队列。 */

    /* 无效档案或无法映射到三个下行端口时拒绝提交。 */
    if(context == RT_NULL) {
        return -RT_EINVAL;
    }

    level = rt_hw_interrupt_disable(); /* 防止提交线程同时修改队列计数和读写下标。 */

    /* 非工作时段拒绝新的控制请求，调用方收到错误后不需要等待异步控制结果。 */
    if(g_inv_data_work_enabled != RT_TRUE) {
        rt_hw_interrupt_enable(level);
        return -RT_EBUSY;
    }

    /* 每个端口最多暂存4项控制请求，队列满时由调用方稍后重试。 */
    if(context->control_queue.count >= INV_CONTROL_QUEUE_SIZE) {
        rt_hw_interrupt_enable(level);
        return -RT_EFULL;
    }

    context->control_queue.requests[context->control_queue.write_index] = *request;
    context->control_queue.write_index = (context->control_queue.write_index + 1U) % INV_CONTROL_QUEUE_SIZE;
    ++context->control_queue.count;
    rt_hw_interrupt_enable(level);
    rt_kprintf("%s archive[%d] control type[%d] request[%d] queued on uart[%d]\n", get_char_time(), request->archive_index + 1, request->type, request->request_id, inv_data_uart_no(context));
    return RT_EOK;
}

/* 等待结果信号量并取出最早生成的一项控制结果。 */
rt_err_t Inv_Control_Get_Result(Inv_Control_Result_Info_t *result, int32_t timeout)
{
    rt_base_t level;      /* 读取公共结果队列前保存的中断级别。 */
    rt_err_t wait_result; /* 等待结果信号量得到的RT-Thread返回码。 */

    /* 输出指针为空或等待时间小于永久等待值时，参数无效。 */
    if(result == RT_NULL) {
        return -RT_EINVAL;
    }

    /* RT_WAITING_FOREVER为允许的最小特殊值，更小的负数没有定义。 */
    if(timeout < RT_WAITING_FOREVER) {
        return -RT_EINVAL;
    }

    /* 结果信号量尚未初始化时不能进入阻塞等待。 */
    if(g_inv_control_result_sem_initialized != RT_TRUE) {
        return -RT_EBUSY;
    }

    /* 先等待结果信号，超时或线程等待被中断时直接返回RT-Thread错误码。 */
    wait_result = rt_sem_take(&g_inv_control_result_sem, timeout); /* 没有结果时按调用方超时时间阻塞。 */
    if(wait_result != RT_EOK) {
        return wait_result;
    }

    level = rt_hw_interrupt_disable(); /* 保证结果内容、读下标和计数同步取出。 */

    /* 正常情况下取得信号后队列必然有结果，队列为空仅可能由重复初始化引起。 */
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
