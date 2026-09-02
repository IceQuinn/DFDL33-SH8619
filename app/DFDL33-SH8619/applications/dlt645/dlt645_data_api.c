#include "dlt645_data_api.h"

#include <stdint.h>
#include <rtthread.h>

#include "inv_data.h"

/* 检查无符号压缩BCD每个半字节是否位于0～9，发现A～F立即判定数据非法。 */
static rt_bool_t dlt645_bcd_is_valid(const uint8_t *data, uint16_t data_len)
{
    uint16_t index; /* 当前检查的BCD字节下标。 */

    for(index = 0U; index < data_len; ++index)
    {
        if(((data[index] & 0x0FU) > 9U) || (((data[index] >> 4) & 0x0FU) > 9U)) /* 高低半字节都必须是十进制数字。 */
        {
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

/* 将低字节在前的无符号压缩BCD解码为int32_t，并拒绝非法BCD或超过int32_t的数值。 */
static rt_err_t dlt645_bcd_decode_u32(const uint8_t *data, uint16_t data_len, int32_t *value)
{
    uint16_t index;            /* 当前解码的BCD字节下标。 */
    uint64_t result = 0U;      /* 使用64位中间值，防止累加阶段先发生32位溢出。 */
    uint64_t multiplier = 1U;  /* 每处理一个字节权重乘100，因为每字节包含两位十进制数。 */

    if(!dlt645_bcd_is_valid(data, data_len)) /* 解码前统一确认所有半字节都是有效BCD。 */
    {
        return -RT_EINVAL;
    }

    for(index = 0U; index < data_len; ++index)
    {
        uint64_t pair = (uint64_t)(data[index] & 0x0FU) +
                        (uint64_t)((data[index] >> 4) & 0x0FU) * 10U; /* 当前字节表示00～99。 */
        result += pair * multiplier; /* 按DL/T 645低地址字节在前的顺序累加当前两位数字。 */
        if(result > INT32_MAX) /* 当前控制接口使用int32_t，超范围数据不能继续下发。 */
        {
            return -RT_EINVAL;
        }
        multiplier *= 100U; /* 下一个BCD字节移动两位十进制数量级。 */
    }

    *value = (int32_t)result; /* 所有范围检查通过后才写入调用方结果变量。 */
    return RT_EOK;
}

/* 读取DI0指定逆变器的推导运行状态，按规范编码为00开机或01关机。 */
rt_err_t dlt645_read_run_state(const Dlt645PointTypeDef *point,
                               uint32_t id,
                               uint8_t *data,
                               uint16_t capacity,
                               uint16_t *data_len)
{
    uint8_t archive_index = (uint8_t)id - 1U;       /* 已校验的DI0从1起始，减1转换为0～11档案下标。 */
    Inv_Data_t *inv_data = Inv_Data_Get(archive_index); /* 通过公共接口取得对应档案实时数据，避免直接索引全局数组。 */

    RT_UNUSED(point); /* 当前运行状态读取不需要倍率等描述字段，保留参数以统一回调签名。 */
    if((inv_data == RT_NULL) || (inv_data->run_state == INV_RUN_STATE_UNKNOWN)) /* 档案无效或状态无法推导时不返回伪造值。 */
    {
        return -RT_ERROR;
    }
    if(capacity < 1U) /* 输出缓冲区必须能容纳规范规定的一字节状态。 */
    {
        return -RT_EINVAL;
    }

    data[0] = (inv_data->run_state == INV_RUN_STATE_ON) ? 0x00U : 0x01U; /* 规范定义0为开机、1为关机。 */
    *data_len = 1U; /* 告知统一组帧层本次生成一个业务数据字节。 */
    return RT_EOK;
}

/* 解码并校验运行状态写数据，提交对应档案的开关机控制并等待最终执行结果。 */
rt_err_t dlt645_write_run_state(const Dlt645PointTypeDef *point,
                                uint32_t id,
                                const uint8_t *data,
                                uint16_t data_len)
{
    int32_t value;                    /* 从一字节BCD解码得到的运行状态枚举值。 */
    Inv_Control_Request_t request;    /* 即将提交到逆变器控制队列的完整请求。 */
    Inv_Control_Result_Info_t result; /* 控制队列返回的最终执行结果和关联请求信息。 */
    rt_err_t control_result;          /* 控制提交或等待结果接口的直接返回码。 */

    if((data_len != point->data_len) ||
       (dlt645_bcd_decode_u32(data, data_len, &value) != RT_EOK) ||
       (value < point->write_min) || (value > point->write_max)) /* 长度、BCD格式和值域任一非法都拒绝执行控制。 */
    {
        return -RT_EINVAL;
    }

    request.request_id = id; /* 使用完整数据标识关联异步控制请求和返回结果。 */
    request.archive_index = (uint8_t)id - 1U; /* 已校验的DI0转换为0～11档案下标。 */
    request.value = value; /* 保存原始状态值，便于结果日志和后续扩展使用。 */
    request.type = (value == 0) ? INV_CONTROL_POWER_ON : INV_CONTROL_POWER_OFF; /* 严格遵循规范0开机、1关机。 */

    control_result = Inv_Control_Submit(&request); /* 将请求提交到目标下行端口的高优先级控制队列。 */
    if(control_result != RT_EOK) /* 提交失败说明档案、协议或控制队列当前不可用。 */
    {
        return control_result;
    }

    control_result = Inv_Control_Get_Result(&result, 1000); /* 最多等待1000个系统tick取得设备最终应答。 */
    if((control_result != RT_EOK) ||
       (result.request.request_id != request.request_id) ||
       (result.result != INV_CONTROL_RESULT_OK)) /* 超时、结果不匹配或设备控制失败均生成645异常应答。 */
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}
