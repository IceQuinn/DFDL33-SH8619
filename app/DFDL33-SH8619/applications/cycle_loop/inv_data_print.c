/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "inv_data_print.h"

#include <stdlib.h>
#include <rtthread.h>

#include "inv_data.h"
#include "inverter_archive.h"
#include "user_rtc.h"

/* 一张表横向显示全部12台逆变器，单元格使用空格分隔。 */
#define INV_DATA_PRINT_GROUP_SIZE           INVERTER_ARCHIVE_MAX_COUNT
#define INV_DATA_PRINT_CELL_WIDTH           12U

/* 数据类各数组对应的打印名称。 */
static const char *g_inv_data_print_u_name[ENUM_PHASE_MAX] = {"Ua", "Ub", "Uc"};
static const char *g_inv_data_print_i_name[ENUM_PHASE_MAX] = {"Ia", "Ib", "Ic"};
static const char *g_inv_data_print_p_name[ENUM_PMAX] = {"Pa", "Pb", "Pc", "Pt"};
static const char *g_inv_data_print_q_name[ENUM_QMAX] = {"Qa", "Qb", "Qc", "Qt"};
static const char *g_inv_data_print_pf_name[ENUM_PFMAX] = {"PFa", "PFb", "PFc", "PFt"};

/* 将档案端口编号转换为表格中使用的短名称。 */
static const char *inv_data_print_port_name(uint8_t port)
{
    switch(port) {
    case INV_PORT_RJ45_1:
        return "RJ45-I";
    case INV_PORT_RJ45_2:
        return "RJ45-II";
    case INV_PORT_RS485_2:
        return "RS485-II";
    case INV_PORT_WIRELESS:
        return "WIRELESS";
    default:
        return "UNKNOWN";
    }
}

/* 打印一个固定宽度单元格，过长文本以~结尾，避免破坏后续列对齐。 */
static void inv_data_print_cell(const char *text)
{
    char display[INV_DATA_PRINT_CELL_WIDTH + 1U];
    rt_size_t length;

    if(text == RT_NULL) {
        text = "--";
    }

    length = rt_strlen(text);
    if(length > INV_DATA_PRINT_CELL_WIDTH) {
        rt_memcpy(display, text, INV_DATA_PRINT_CELL_WIDTH - 1U);
        display[INV_DATA_PRINT_CELL_WIDTH - 1U] = '~';
        display[INV_DATA_PRINT_CELL_WIDTH] = '\0';
        text = display;
    }

    rt_kprintf(" %-12s", text);
}

static void inv_data_print_row_begin(const char *name)
{
    rt_kprintf("%-20s ", name);
}

static void inv_data_print_row_end(void)
{
    rt_kprintf("\n");
}

/* 打印当前分组的逆变器编号表头。 */
static void inv_data_print_header(uint8_t first_archive, uint8_t archive_count)
{
    uint8_t offset;
    char inverter_name[16];

    inv_data_print_row_begin("Item");
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_snprintf(inverter_name, sizeof(inverter_name), "INV%d", first_archive + offset + 1U);
        inv_data_print_cell(inverter_name);
    }
    inv_data_print_row_end();
}

/* 打印数值型实时数据行，无效档案显示--，无效实时值显示INVALID。 */
static void inv_data_print_value_row(const char *name,
                                     const Inv_RealtimeValue_t *const sources[],
                                     const uint8_t archive_valid[],
                                     uint8_t archive_count)
{
    Inv_RealtimeValue_t values[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;
    char text[24];

    rt_enter_critical();
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_memcpy(&values[offset], sources[offset], sizeof(values[offset]));
    }
    rt_exit_critical();

    inv_data_print_row_begin(name);
    for(offset = 0U; offset < archive_count; ++offset) {
        if(archive_valid[offset] != INVERTER_ARCHIVE_VALID) {
            inv_data_print_cell("--");
        }
        else if(values[offset].valid == 0U) {
            inv_data_print_cell("INVALID");
        }
        else {
            rt_snprintf(text, sizeof(text), "%d", values[offset].value);
            inv_data_print_cell(text);
        }
    }
    inv_data_print_row_end();
}

/* 打印设备编号实时数据行。 */
static void inv_data_print_string_row(const char *name,
                                      const Inv_RealtimeString_t *const sources[],
                                      const uint8_t archive_valid[],
                                      uint8_t archive_count)
{
    Inv_RealtimeString_t values[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;

    rt_enter_critical();
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_memcpy(&values[offset], sources[offset], sizeof(values[offset]));
    }
    rt_exit_critical();

    inv_data_print_row_begin(name);
    for(offset = 0U; offset < archive_count; ++offset) {
        if(archive_valid[offset] != INVERTER_ARCHIVE_VALID) {
            inv_data_print_cell("--");
        }
        else if(values[offset].valid == 0U) {
            inv_data_print_cell("INVALID");
        }
        else {
            values[offset].value[INV_DATA_DEVICE_NO_MAX_LEN] = '\0';
            inv_data_print_cell(values[offset].value);
        }
    }
    inv_data_print_row_end();
}

/* 打印逆变器运行状态，状态值与对外约定的0、1和0xFF保持一致。 */
static void inv_data_print_run_state_row(uint8_t first_archive,
                                         const uint8_t archive_valid[],
                                         uint8_t archive_count)
{
    Inv_Run_State_t states[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;

    rt_enter_critical();
    for(offset = 0U; offset < archive_count; ++offset) {
        states[offset] = g_inv_data[first_archive + offset].run_state;
    }
    rt_exit_critical();

    inv_data_print_row_begin("Run state");
    for(offset = 0U; offset < archive_count; ++offset) {
        if(archive_valid[offset] != INVERTER_ARCHIVE_VALID) {
            inv_data_print_cell("--");
        }
        else if(states[offset] == INV_RUN_STATE_OFF) {
            inv_data_print_cell("0(OFF)");
        }
        else if(states[offset] == INV_RUN_STATE_ON) {
            inv_data_print_cell("1(ON)");
        }
        else {
            inv_data_print_cell("0xFF(UNKNOWN)");
        }
    }
    inv_data_print_row_end();
}

/* 打印当前分组的档案字段。 */
static void inv_data_print_archive_rows(const Inv_Archive_t archives[],
                                        const uint8_t archive_valid[],
                                        uint8_t archive_count)
{
    uint8_t offset;
    char text[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 16U];
    char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U];

    rt_kprintf("[ARCHIVE]\n");

    inv_data_print_row_begin("Valid");
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_snprintf(text, sizeof(text), "%s(%d)",
                    (archive_valid[offset] == INVERTER_ARCHIVE_VALID) ? "VALID" : "INVALID",
                    archive_valid[offset]);
        inv_data_print_cell(text);
    }
    inv_data_print_row_end();

    inv_data_print_row_begin("Modbus address");
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_snprintf(text, sizeof(text), "%d", archives[offset].mb_addr);
        inv_data_print_cell(text);
    }
    inv_data_print_row_end();

    inv_data_print_row_begin("Manufacturer");
    for(offset = 0U; offset < archive_count; ++offset) {
        Inv_Archive_Copy_Mfr_Name(manufacturer, archives[offset].mfr_info.name);
        inv_data_print_cell((manufacturer[0] == '\0') ? "(empty)" : manufacturer);
    }
    inv_data_print_row_end();

    inv_data_print_row_begin("Protocol version");
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_snprintf(text, sizeof(text), "0x%04X", (unsigned int)archives[offset].mfr_info.proto_ver);
        inv_data_print_cell(text);
    }
    inv_data_print_row_end();

    inv_data_print_row_begin("Access port");
    for(offset = 0U; offset < archive_count; ++offset) {
        rt_snprintf(text, sizeof(text), "%d(%s)", archives[offset].port,
                    inv_data_print_port_name(archives[offset].port));
        inv_data_print_cell(text);
    }
    inv_data_print_row_end();
}

/* 打印当前分组的数据类实时数据。 */
static void inv_data_print_data_rows(uint8_t first_archive,
                                     uint8_t archive_count,
                                     const uint8_t archive_valid[])
{
    const Inv_RealtimeValue_t *sources[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t field;
    uint8_t offset;

    rt_kprintf("\n[DATA]\n");

    inv_data_print_run_state_row(first_archive, archive_valid, archive_count);

    for(field = 0U; field < ENUM_PHASE_MAX; ++field) {
        for(offset = 0U; offset < archive_count; ++offset) {
            sources[offset] = &g_inv_data[first_archive + offset].data.Ux[field];
        }
        inv_data_print_value_row(g_inv_data_print_u_name[field], sources, archive_valid, archive_count);
    }

    for(field = 0U; field < ENUM_PHASE_MAX; ++field) {
        for(offset = 0U; offset < archive_count; ++offset) {
            sources[offset] = &g_inv_data[first_archive + offset].data.Ix[field];
        }
        inv_data_print_value_row(g_inv_data_print_i_name[field], sources, archive_valid, archive_count);
    }

    for(field = 0U; field < ENUM_PMAX; ++field) {
        for(offset = 0U; offset < archive_count; ++offset) {
            sources[offset] = &g_inv_data[first_archive + offset].data.Px[field];
        }
        inv_data_print_value_row(g_inv_data_print_p_name[field], sources, archive_valid, archive_count);
    }

    for(field = 0U; field < ENUM_QMAX; ++field) {
        for(offset = 0U; offset < archive_count; ++offset) {
            sources[offset] = &g_inv_data[first_archive + offset].data.Qx[field];
        }
        inv_data_print_value_row(g_inv_data_print_q_name[field], sources, archive_valid, archive_count);
    }

    for(field = 0U; field < ENUM_PFMAX; ++field) {
        for(offset = 0U; offset < archive_count; ++offset) {
            sources[offset] = &g_inv_data[first_archive + offset].data.PFx[field];
        }
        inv_data_print_value_row(g_inv_data_print_pf_name[field], sources, archive_valid, archive_count);
    }

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].daily_energy;
    }
    inv_data_print_value_row("Daily generation", sources, archive_valid, archive_count);
}

/* 打印当前分组的参数类实时数据。 */
static void inv_data_print_param_rows(uint8_t first_archive,
                                      uint8_t archive_count,
                                      const uint8_t archive_valid[])
{
    const Inv_RealtimeValue_t *number_sources[INV_DATA_PRINT_GROUP_SIZE];
    const Inv_RealtimeString_t *string_sources[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;

    rt_kprintf("\n[PARAMETER]\n");

    for(offset = 0U; offset < archive_count; ++offset) {
        string_sources[offset] = &g_inv_data[first_archive + offset].param.dev_no;
    }
    inv_data_print_string_row("Device number", string_sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        number_sources[offset] = &g_inv_data[first_archive + offset].param.Pn;
    }
    inv_data_print_value_row("Pn", number_sources, archive_valid, archive_count); /* 额定有功功率打印名称与协议库字段Pn保持一致。 */

    for(offset = 0U; offset < archive_count; ++offset) {
        number_sources[offset] = &g_inv_data[first_archive + offset].param.Qn;
    }
    inv_data_print_value_row("Qn", number_sources, archive_valid, archive_count); /* 额定无功功率打印名称与协议库字段Qn保持一致。 */

    for(offset = 0U; offset < archive_count; ++offset) {
        number_sources[offset] = &g_inv_data[first_archive + offset].param.set_volt;
    }
    inv_data_print_value_row("Set voltage", number_sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        number_sources[offset] = &g_inv_data[first_archive + offset].param.output_type;
    }
    inv_data_print_value_row("Output type", number_sources, archive_valid, archive_count);
}

/* 打印当前分组的五个可回读控制类实时数据。 */
static void inv_data_print_control_rows(uint8_t first_archive,
                                        uint8_t archive_count,
                                        const uint8_t archive_valid[])
{
    const Inv_RealtimeValue_t *sources[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;

    rt_kprintf("\n[CONTROL]\n");

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].ctrl.active_pwr_ctrl;
    }
    inv_data_print_value_row("Active power", sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].ctrl.reactive_pwr_ctrl;
    }
    inv_data_print_value_row("Reactive power", sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].ctrl.pwr_factor_ctrl;
    }
    inv_data_print_value_row("Power factor", sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].ctrl.active_pwr_pct_ctrl;
    }
    inv_data_print_value_row("Active power pct", sources, archive_valid, archive_count);

    for(offset = 0U; offset < archive_count; ++offset) {
        sources[offset] = &g_inv_data[first_archive + offset].ctrl.reactive_pwr_pct_ctrl;
    }
    inv_data_print_value_row("Reactive power pct", sources, archive_valid, archive_count);
}

/* 以字段为行、逆变器为列打印一个档案分组。 */
static void inv_data_print_group(uint8_t first_archive, uint8_t archive_count)
{
    Inv_Archive_t archives[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t archive_valid[INV_DATA_PRINT_GROUP_SIZE];
    uint8_t offset;

    rt_enter_critical();
    for(offset = 0U; offset < archive_count; ++offset) {
        archive_valid[offset] = g_inv_archive_lib.valid[first_archive + offset];
        rt_memcpy(&archives[offset], &g_inv_archive_lib.archives[first_archive + offset],
                  sizeof(archives[offset]));
    }
    rt_exit_critical();

    rt_kprintf("%s Inverter realtime data: INV%d-INV%d\n", get_char_time(),
               first_archive + 1U, first_archive + archive_count);
    inv_data_print_header(first_archive, archive_count);
    inv_data_print_archive_rows(archives, archive_valid, archive_count);
    inv_data_print_data_rows(first_archive, archive_count, archive_valid);
    inv_data_print_param_rows(first_archive, archive_count, archive_valid);
    inv_data_print_control_rows(first_archive, archive_count, archive_valid);
    rt_kprintf("\n");
}

/* 使用一张无边框横向表格打印全部12个档案槽位。 */
void Inv_Data_Print_All(void)
{
    inv_data_print_group(0U, INVERTER_ARCHIVE_MAX_COUNT);
}

/* 按1～12档案编号打印单台逆变器的纵向字段表格。 */
void Inv_Data_Print_Archive(uint8_t archive_number)
{
    if((archive_number == 0U) || (archive_number > INVERTER_ARCHIVE_MAX_COUNT)) {
        rt_kprintf("%s archive number[%d] invalid, expected 1-12\n", get_char_time(), archive_number);
        return;
    }

    inv_data_print_group(archive_number - 1U, 1U);
}

/* MSH命令入口，无参数打印全部档案，提供参数时打印指定档案。 */
static int inv_data_print(int argc, char **argv)
{
    char *end_ptr;
    int32_t archive_number;
    int32_t max_archive_number = INVERTER_ARCHIVE_MAX_COUNT;
    int64_t parsed_archive_number;

    if(argc == 1) {
        Inv_Data_Print_All();
        return 0;
    }

    if(argc != 2) {
        rt_kprintf("%s usage: inv_data_print [archive:1-12]\n", get_char_time());
        return -1;
    }

    parsed_archive_number = strtol(argv[1], &end_ptr, 10);
    if((end_ptr == argv[1]) || (*end_ptr != '\0') ||
       (parsed_archive_number < 1) || (parsed_archive_number > max_archive_number)) {
        rt_kprintf("%s archive number[%s] is invalid, expected 1-12\n", get_char_time(), argv[1]);
        return -1;
    }

    archive_number = (int32_t)parsed_archive_number;
    Inv_Data_Print_Archive((uint8_t)archive_number);
    return 0;
}
MSH_CMD_EXPORT(inv_data_print, print inverter realtime data);


void Inv_Control_Test(void)
{
    Inv_Control_Request_t request;   /* 本次MSH测试提交的开机控制请求。 */
    Inv_Control_Result_Info_t result; /* 等待信号量后取得的异步控制结果。 */
    rt_err_t control_result;          /* 控制提交或等待结果接口的返回码。 */

    request.request_id = 2U;
    request.archive_index = 0U;
    request.type = INV_CONTROL_POWER_ON;
    request.value = 0; /* 开机使用协议库默认写入值，该字段不会参与组帧。 */

    control_result = Inv_Control_Submit(&request); /* 先确认请求是否成功进入对应端口控制队列。 */

    /* 请求没有进入控制队列时直接打印提交错误，不再等待结果信号量。 */
    if(control_result != RT_EOK) {
        rt_kprintf("%s request[%d] submit failed, result[%d]\n", get_char_time(), request.request_id, control_result);
        return;
    }

    /* MSH测试线程最多等待3秒，结果生成后信号量会立即唤醒本线程。 */
    control_result = Inv_Control_Get_Result(&result, 3000); /* 最多等待3秒取得最早完成的控制结果。 */
    /* 等待超时、信号量错误或结果接口失败时结束本次测试。 */
    if(control_result != RT_EOK) {
        rt_kprintf("%s request[%d] wait failed, result[%d]\n", get_char_time(), request.request_id, control_result);
        return;
    }

    /* request_id用于确认取出的异步结果是否属于本次测试命令。 */
    if(result.request.request_id != request.request_id) {
        rt_kprintf("%s request[%d] got another request[%d] result\n", get_char_time(), request.request_id, result.request.request_id);
        return;
    }

    rt_kprintf("%s request[%d] result[%d] exception[%d]\n", get_char_time(), result.request.request_id, result.result, result.exception_code);
}
MSH_CMD_EXPORT(Inv_Control_Test, Inv_Control_Test);
