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

/* 数据类各数组对应的打印名称。 */
static const char *g_inv_data_print_u_name[ENUM_PHASE_MAX] = {"Ua", "Ub", "Uc"}; /* 三相电压数组下标对应的名称。 */
static const char *g_inv_data_print_i_name[ENUM_PHASE_MAX] = {"Ia", "Ib", "Ic"}; /* 三相电流数组下标对应的名称。 */
static const char *g_inv_data_print_p_name[ENUM_PMAX] = {"Pa", "Pb", "Pc", "Pt"}; /* 分相及总有功功率名称。 */
static const char *g_inv_data_print_q_name[ENUM_QMAX] = {"Qa", "Qb", "Qc", "Qt"}; /* 分相及总无功功率名称。 */
static const char *g_inv_data_print_pf_name[ENUM_PFMAX] = {"PFa", "PFb", "PFc", "PFt"}; /* 分相及总功率因数名称。 */

/* 打印一个数值型实时数据项，无效数据明确打印INVALID。 */
static void inv_data_print_value(uint8_t archive_number,
                                 const char *data_class,
                                 const char *name,
                                 const Inv_RealtimeValue_t *value)
{
    /* 数据有效时打印int32_t实时值和最近一次成功更新时间。 */
    if(value->valid != 0U) {
        rt_kprintf("%s archive[%d] %s.%s value[%d] update_tick[%d]\n", get_char_time(), archive_number, data_class, name, value->value, value->update_tick);
    }
    /* 数据尚未读取成功、读取失败或协议未配置时打印INVALID。 */
    else {
        rt_kprintf("%s archive[%d] %s.%s INVALID\n", get_char_time(), archive_number, data_class, name);
    }
}

/* 打印设备编号字符串实时数据，无效数据明确打印INVALID。 */
static void inv_data_print_string(uint8_t archive_number,
                                  const char *data_class,
                                  const char *name,
                                  const Inv_RealtimeString_t *value)
{
    /* 字符串有效时打印内容、有效长度和最近一次成功更新时间。 */
    if(value->valid != 0U) {
        rt_kprintf("%s archive[%d] %s.%s value[%s] length[%d] update_tick[%d]\n", get_char_time(), archive_number, data_class, name, value->value, value->length, value->update_tick);
    }
    /* 字符串尚未读取成功、读取失败或协议未配置时打印INVALID。 */
    else {
        rt_kprintf("%s archive[%d] %s.%s INVALID\n", get_char_time(), archive_number, data_class, name);
    }
}

/* 打印数据类中的电压、电流、有功、无功和功率因数实时数据。 */
static void inv_data_print_data_class(uint8_t archive_number, const Inv_RealtimeData_t *data)
{
    uint8_t index; /* 当前正在打印的数据数组下标。 */

    /* 打印A、B、C三相电压。 */
    for(index = 0U; index < ENUM_PHASE_MAX; ++index) {
        inv_data_print_value(archive_number, "data", g_inv_data_print_u_name[index], &data->Ux[index]);
    }

    /* 打印A、B、C三相电流。 */
    for(index = 0U; index < ENUM_PHASE_MAX; ++index) {
        inv_data_print_value(archive_number, "data", g_inv_data_print_i_name[index], &data->Ix[index]);
    }

    /* 打印三相及总有功功率。 */
    for(index = 0U; index < ENUM_PMAX; ++index) {
        inv_data_print_value(archive_number, "data", g_inv_data_print_p_name[index], &data->Px[index]);
    }

    /* 打印三相及总无功功率。 */
    for(index = 0U; index < ENUM_QMAX; ++index) {
        inv_data_print_value(archive_number, "data", g_inv_data_print_q_name[index], &data->Qx[index]);
    }

    /* 打印三相及总功率因数。 */
    for(index = 0U; index < ENUM_PFMAX; ++index) {
        inv_data_print_value(archive_number, "data", g_inv_data_print_pf_name[index], &data->PFx[index]);
    }
}

/* 打印参数类中的设备编号、额定功率、电压、输出类型和开关机状态。 */
static void inv_data_print_param_class(uint8_t archive_number, const Inv_RealtimeParam_t *param)
{
    /* 参数类字段数量固定，按照协议定义顺序逐项打印有效值或INVALID。 */
    inv_data_print_string(archive_number, "param", "device_no", &param->dev_no);
    inv_data_print_value(archive_number, "param", "pv_rated_p", &param->pv_rated_active_pwr);
    inv_data_print_value(archive_number, "param", "pv_rated_q", &param->pv_rated_reactive_pwr);
    inv_data_print_value(archive_number, "param", "set_voltage", &param->set_volt);
    inv_data_print_value(archive_number, "param", "output_type", &param->output_type);
    inv_data_print_value(archive_number, "param", "power_status", &param->pwr_status);
}

/* 打印控制类中的开关机及功率控制寄存器实时数据。 */
static void inv_data_print_ctrl_class(uint8_t archive_number, const Inv_RealtimeCtrl_t *ctrl)
{
    /* 控制类字段数量固定，按照通用控制类型顺序逐项打印。 */
    inv_data_print_value(archive_number, "ctrl", "power_on", &ctrl->pwr_on);
    inv_data_print_value(archive_number, "ctrl", "power_off", &ctrl->pwr_off);
    inv_data_print_value(archive_number, "ctrl", "active_pwr_ctrl", &ctrl->active_pwr_ctrl);
    inv_data_print_value(archive_number, "ctrl", "reactive_pwr_ctrl", &ctrl->reactive_pwr_ctrl);
    inv_data_print_value(archive_number, "ctrl", "power_factor_ctrl", &ctrl->pwr_factor_ctrl);
    inv_data_print_value(archive_number, "ctrl", "active_pwr_pct_ctrl", &ctrl->active_pwr_pct_ctrl);
    inv_data_print_value(archive_number, "ctrl", "reactive_pwr_pct_ctrl", &ctrl->reactive_pwr_pct_ctrl);
}

/* 打印指定档案槽位，档案有效时使用实时数据快照打印全部31项数据。 */
static void inv_data_print_archive_index(uint8_t archive_index)
{
    Inv_Archive_t archive; /* 临界区内取得的单个档案配置快照。 */
    Inv_Data_t data;       /* 临界区内取得的单个档案实时数据快照。 */
    char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U]; /* 补字符串结束符后的厂家名称。 */
    uint8_t archive_number = archive_index + 1U; /* 面向日志显示的1～12档案编号。 */
    uint8_t archive_valid; /* 当前档案槽位的有效标志快照。 */

    /* 临界区内复制有效标志、档案和实时数据，耗时打印在退出临界区后执行。 */
    rt_enter_critical();
    archive_valid = g_inv_archive_lib.valid[archive_index];
    rt_memcpy(&archive, &g_inv_archive_lib.archives[archive_index], sizeof(archive));
    rt_memcpy(&data, &g_inv_data[archive_index], sizeof(data));
    rt_exit_critical();

    /* 无效档案只打印档案编号和INVALID，不继续打印实时数据项。 */
    if(archive_valid != INVERTER_ARCHIVE_VALID) {
        rt_kprintf("%s archive[%d] INVALID\n", get_char_time(), archive_number);
        return;
    }

    Inv_Archive_Copy_Mfr_Name(manufacturer, archive.mfr_info.name); /* 将Flash定长厂家字段转换为安全C字符串。 */
    rt_kprintf("%s archive[%d] VALID addr[%d] port[%d] manufacturer[%s] protocol[0x%04X]\n", get_char_time(), archive_number, archive.mb_addr, archive.port, manufacturer, (unsigned int)archive.mfr_info.proto_ver);
    inv_data_print_data_class(archive_number, &data.data);
    inv_data_print_param_class(archive_number, &data.param);
    inv_data_print_ctrl_class(archive_number, &data.ctrl);
}

/* 打印全部12个档案槽位，有效档案打印实时数据，无效档案打印INVALID。 */
void Inv_Data_Print_All(void)
{
    uint8_t archive_index; /* 当前正在打印的0～11档案槽位下标。 */

    /* 固定遍历全部档案槽位，不能只按有效档案count打印。 */
    for(archive_index = 0U; archive_index < INVERTER_ARCHIVE_MAX_COUNT; ++archive_index) {
        inv_data_print_archive_index(archive_index);
    }
}

/* 按1～12档案编号打印单台逆变器实时数据。 */
void Inv_Data_Print_Archive(uint8_t archive_number)
{
    /* 档案编号从1开始，0或超过12时打印参数错误。 */
    if((archive_number == 0U) || (archive_number > INVERTER_ARCHIVE_MAX_COUNT)) {
        rt_kprintf("%s archive number[%d] invalid, expected 1-12\n", get_char_time(), archive_number);
        return;
    }

    inv_data_print_archive_index(archive_number - 1U);
}

/* MSH命令入口，无参数打印全部档案，提供参数时打印指定档案。 */
static int inv_data_print(int argc, char **argv)
{
    char *end_ptr;             /* strtol返回的首个未解析字符地址。 */
    int32_t archive_number;    /* 校验通过后转换为int32_t的档案编号。 */
    int32_t max_archive_number = INVERTER_ARCHIVE_MAX_COUNT; /* MSH允许输入的最大档案编号。 */
    int64_t parsed_archive_number; /* strtol解析出的宽范围临时数值。 */

    /* 未提供参数时打印全部12个档案槽位。 */
    if(argc == 1) {
        Inv_Data_Print_All();
        return 0;
    }

    /* 参数数量不是一个时打印正确命令格式。 */
    if(argc != 2) {
        rt_kprintf("%s usage: inv_data_print [archive:1-12]\n", get_char_time());
        return -1;
    }

    parsed_archive_number = strtol(argv[1], &end_ptr, 10);

    /* 参数必须是完整十进制数字，并且位于1～12档案编号范围内。 */
    if((end_ptr == argv[1]) || (*end_ptr != '\0') ||
       (parsed_archive_number < 1) || (parsed_archive_number > max_archive_number)) {
        rt_kprintf("%s archive number[%s] is invalid, expected 1-12\n", get_char_time(), argv[1]);
        return -1;
    }

    archive_number = (int32_t)parsed_archive_number;
    Inv_Data_Print_Archive((uint8_t)archive_number); /* 参数已校验为1～12，可以安全转换后打印。 */
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
