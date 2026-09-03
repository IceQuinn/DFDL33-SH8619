#include "dlt645_data_api.h"

#include <stdint.h>
#include <rtthread.h>

#include "inv_data.h"
#include "inverter_protocol_library.h"

#define DLT645_VARIABLE_ALL_SELECTOR  0xFFU /* DI0等于FF时按档案顺序返回全部12台逆变器。 */
#define DLT645_VOLTAGE_LEN            6U    /* 三相电压各占2字节，单台合计6字节。 */
#define DLT645_CURRENT_LEN            9U    /* 三相电流各占3字节，单台合计9字节。 */
#define DLT645_POWER_LEN              16U   /* 总、A、B、C相功率各占4字节，单台合计16字节。 */
#define DLT645_POWER_FACTOR_LEN       8U    /* 总、A、B、C相功率因数各占2字节，单台合计8字节。 */
#define DLT645_ALL_VARIABLE_LEN       55U   /* 五组变量按规范顺序拼接后的单台总长度。 */

typedef enum Dlt645VariableType
{
    DLT645_VARIABLE_VOLTAGE = 0, /* 三相电压，目标格式XXX.X。 */
    DLT645_VARIABLE_CURRENT,     /* 三相电流，目标格式XXXX.XX。 */
    DLT645_VARIABLE_ACTIVE,      /* 总、A、B、C相有功功率，目标格式XXXX.XXXX。 */
    DLT645_VARIABLE_REACTIVE,    /* 总、A、B、C相无功功率，目标格式XXXX.XXXX。 */
    DLT645_VARIABLE_FACTOR,      /* 总、A、B、C相功率因数，目标格式X.XXX。 */
    DLT645_VARIABLE_ALL          /* 单台逆变器全部变量的组合数据块。 */
} Dlt645VariableTypeDef;

typedef enum Dlt645NominalPowerType
{
    DLT645_NOMINAL_POWER_PN = 0, /* 额定有功功率Pn。 */
    DLT645_NOMINAL_POWER_QN      /* 额定无功功率Qn。 */
} Dlt645NominalPowerTypeDef;

/* 计算10的指定次幂，协议配置小数位超过9时返回0表示配置不可转换。 */
static uint32_t dlt645_decimal_scale(uint8_t decimal_places)
{
    uint8_t index;       /* 当前已经累计的小数位数。 */
    uint32_t scale = 1U; /* 从10的0次幂开始逐位扩大倍率。 */

    if(decimal_places > 9U) /* int32_t定点数据不支持超过9位的小数倍率。 */
    {
        return 0U;
    }
    for(index = 0U; index < decimal_places; ++index)
    {
        scale *= 10U; /* 每增加一位小数，定点整数倍率扩大10倍。 */
    }
    return scale;
}

/* 将协议实时值的小数位统一换算为645点表要求的小数位，缩小时直接舍去最低位。 */
static rt_err_t dlt645_rescale_value(int32_t source, uint8_t source_decimals, uint8_t target_decimals, int32_t *target)
{
    uint32_t scale;       /* 源与目标小数位差对应的10次幂。 */
    int64_t scaled_value; /* 使用64位中间值防止扩大倍率时发生32位溢出。 */

    if(source_decimals == target_decimals) /* 小数位一致时不重复执行乘除运算。 */
    {
        *target = source;
        return RT_EOK;
    }
    scale = dlt645_decimal_scale((source_decimals > target_decimals) ?
                                 (source_decimals - target_decimals) :
                                 (target_decimals - source_decimals)); /* 只计算小数位差所需的倍率。 */
    if(scale == 0U) /* 非法协议小数位不能生成可信的645数据。 */
    {
        return -RT_EINVAL;
    }
    scaled_value = source;
    if(source_decimals > target_decimals) /* 源精度更高时缩小到点表规定的小数位。 */
    {
        scaled_value /= scale;
    }
    else /* 源精度更低时扩大定点整数以补足点表小数位。 */
    {
        scaled_value *= scale;
    }
    if((scaled_value > INT32_MAX) || (scaled_value < INT32_MIN)) /* 换算结果超出实时值表示范围时判定无效。 */
    {
        return -RT_EINVAL;
    }
    *target = (int32_t)scaled_value; /* 完成范围确认后再输出换算结果。 */
    return RT_EOK;
}

/* 将已按目标小数位缩放的整数编码为低字节在前的BCD，signed_value为真时最高位表示负号。 */
static rt_err_t dlt645_encode_bcd(int32_t value, uint8_t *data, uint8_t byte_len, rt_bool_t signed_value)
{
    uint8_t index;       /* 当前写入的BCD字节下标。 */
    uint64_t magnitude;  /* 数值绝对值，使用64位兼容INT32_MIN。 */
    uint64_t limit = 1U; /* 当前BCD字节数能够表达的十进制上限加1。 */

    if((!signed_value) && (value < 0)) /* 电压、电流和功率因数不允许编码负数。 */
    {
        return -RT_EINVAL;
    }
    magnitude = (value < 0) ? (uint64_t)(-(int64_t)value) : (uint64_t)value; /* 避免直接对INT32_MIN取负。 */
    for(index = 0U; index < byte_len; ++index)
    {
        limit *= 100U; /* 每个BCD字节增加两位十进制表示能力。 */
    }
    if((magnitude >= limit) || (signed_value && (magnitude >= (limit * 8U / 10U)))) /* 有符号BCD最高数字只能为0～7。 */
    {
        return -RT_EINVAL;
    }
    for(index = 0U; index < byte_len; ++index)
    {
        data[index] = (uint8_t)((magnitude % 10U) | (((magnitude / 10U) % 10U) << 4)); /* 每字节依次写入低位两位数字。 */
        magnitude /= 100U; /* 移除已经写入的两位十进制数字。 */
    }
    if(signed_value && (value < 0)) /* 负功率使用最高有效字节bit7携带符号。 */
    {
        data[byte_len - 1U] |= 0x80U;
    }
    return RT_EOK;
}

/* 编码一个实时数值，无效、溢出或格式不支持时按点表约定将该字段全部填FF。 */
static void dlt645_append_value(const Inv_RealtimeValue_t *source, uint8_t source_decimals,
                                uint8_t target_decimals, uint8_t byte_len, rt_bool_t signed_value, uint8_t *data)
{
    int32_t scaled_value; /* 完成源协议到645目标精度换算后的定点整数。 */

    if((source->valid == 0U) ||
       (dlt645_rescale_value(source->value, source_decimals, target_decimals, &scaled_value) != RT_EOK) ||
       (dlt645_encode_bcd(scaled_value, data, byte_len, signed_value) != RT_EOK)) /* 任一有效性或编码检查失败都不能返回旧值。 */
    {
        rt_memset(data, 0xFF, byte_len); /* FF明确表示本字段当前无有效数据。 */
    }
}

/* 按变量类型生成单台逆变器的数据块，调用前已经确认实时数据和协议配置有效。 */
static uint16_t dlt645_build_device_variables(Dlt645VariableTypeDef type, const Inv_Data_t *inv,
                                               const Inv_Proto_t *protocol, uint8_t *data)
{
    static const uint8_t power_index[4] = {ENUM_PT, ENUM_PA, ENUM_PB, ENUM_PC}; /* 645功率字段固定按总、A、B、C顺序。 */
    static const uint8_t reactive_index[4] = {ENUM_QT, ENUM_QA, ENUM_QB, ENUM_QC}; /* 无功字段固定按总、A、B、C顺序。 */
    static const uint8_t factor_index[4] = {ENUM_PFT, ENUM_PFA, ENUM_PFB, ENUM_PFC}; /* 功率因数字段固定按总、A、B、C顺序。 */
    uint8_t index;   /* 当前相或当前总/分相字段下标。 */
    uint16_t offset = 0U; /* 当前数据块已经写入的字节数。 */

    if((type == DLT645_VARIABLE_VOLTAGE) || (type == DLT645_VARIABLE_ALL)) /* 电压块位于全部变量块首部。 */
    {
        for(index = 0U; index < ENUM_PHASE_MAX; ++index)
        {
            dlt645_append_value(&inv->data.Ux[index], protocol->data.Ux[index].decimal_places, 1U, 2U, RT_FALSE, &data[offset]);
            offset += 2U; /* 每相电压格式XXX.X固定占2字节。 */
        }
        if(type != DLT645_VARIABLE_ALL) return offset; /* 单项读取完成后直接返回，避免继续拼接其他变量。 */
    }
    if((type == DLT645_VARIABLE_CURRENT) || (type == DLT645_VARIABLE_ALL)) /* 电流块紧随电压块。 */
    {
        for(index = 0U; index < ENUM_PHASE_MAX; ++index)
        {
            dlt645_append_value(&inv->data.Ix[index], protocol->data.Ix[index].decimal_places, 2U, 3U, RT_FALSE, &data[offset]);
            offset += 3U; /* 每相电流格式XXXX.XX固定占3字节。 */
        }
        if(type != DLT645_VARIABLE_ALL) return offset; /* 单项读取只生成当前变量块。 */
    }
    if((type == DLT645_VARIABLE_ACTIVE) || (type == DLT645_VARIABLE_ALL)) /* 有功功率块按总、A、B、C排列。 */
    {
        for(index = 0U; index < 4U; ++index)
        {
            uint8_t item = power_index[index]; /* 将645字段顺序转换为实时数据数组下标。 */
            dlt645_append_value(&inv->data.Px[item], protocol->data.Px[item].decimal_places, 4U, 4U, RT_TRUE, &data[offset]);
            offset += 4U; /* 每项有功功率格式XXXX.XXXX固定占4字节。 */
        }
        if(type != DLT645_VARIABLE_ALL) return offset; /* 单项读取只生成当前变量块。 */
    }
    if((type == DLT645_VARIABLE_REACTIVE) || (type == DLT645_VARIABLE_ALL)) /* 无功功率块按总、A、B、C排列。 */
    {
        for(index = 0U; index < 4U; ++index)
        {
            uint8_t item = reactive_index[index]; /* 将645字段顺序转换为实时数据数组下标。 */
            dlt645_append_value(&inv->data.Qx[item], protocol->data.Qx[item].decimal_places, 4U, 4U, RT_TRUE, &data[offset]);
            offset += 4U; /* 每项无功功率格式XXXX.XXXX固定占4字节。 */
        }
        if(type != DLT645_VARIABLE_ALL) return offset; /* 单项读取只生成当前变量块。 */
    }
    for(index = 0U; index < 4U; ++index) /* 功率因数是单项或全部变量块的最后一组。 */
    {
        uint8_t item = factor_index[index]; /* 将645字段顺序转换为实时数据数组下标。 */
        dlt645_append_value(&inv->data.PFx[item], protocol->data.PFx[item].decimal_places, 3U, 2U, RT_FALSE, &data[offset]);
        offset += 2U; /* 每项功率因数格式X.XXX固定占2字节。 */
    }
    return offset;
}

/* 统一处理变量类单台及DI0=FF聚合读取，公共入口只在此处检查一次重要参数。 */
static rt_err_t dlt645_read_variables(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                      uint16_t capacity, uint16_t *data_len, Dlt645VariableTypeDef type)
{
    uint8_t selector = (uint8_t)id; /* 数据标识最低字节用于选择档案或全部档案。 */
    uint8_t first_archive;          /* 本次读取的首个档案下标。 */
    uint8_t archive_count;          /* 本次需要依次生成的数据块数量。 */
    uint8_t archive_offset;         /* 当前处理相对首档案的偏移。 */
    uint16_t required_len;          /* 当前选择器对应的完整返回长度。 */
    uint16_t offset = 0U;           /* 已经写入输出缓冲区的数据字节数。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针必须有效。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由分发层验证后转换为档案下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF固定聚合全部档案槽位。 */
    required_len = point->data_len * archive_count; /* 点描述保存单台长度，聚合时按12台计算。 */
    if(capacity < required_len) /* 组帧缓存不足时不能生成截断数据。 */
    {
        return -RT_EINVAL;
    }
    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前需要读取的实际档案下标。 */
        Inv_Data_t *inv = Inv_Data_Get(archive_index); /* 通过档案公共接口取得实时数据。 */
        const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得源定点值对应的小数位配置。 */

        if((inv == RT_NULL) || (protocol == RT_NULL)) /* 下挂逆变器为空或未匹配协议时，当前逆变器的全部读取数据统一填FF。 */
        {
            rt_memset(&data[offset], 0xFF, point->data_len); /* 单台和聚合读取都返回固定长度，避免空档案触发645异常应答。 */
            offset += point->data_len;
            continue;
        }
        offset += dlt645_build_device_variables(type, inv, protocol, &data[offset]); /* 有效档案按规范字段顺序编码。 */
    }
    *data_len = offset; /* 所有档案处理完成后一次性返回实际长度。 */
    return (offset == required_len) ? RT_EOK : -RT_ERROR; /* 防止点表长度和编码实现不一致时发送错误报文。 */
}

/* 读取三相电压变量数据。 */
rt_err_t dlt645_read_voltage(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_VOLTAGE);
}

/* 读取三相电流变量数据。 */
rt_err_t dlt645_read_current(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_CURRENT);
}

/* 读取总、A、B、C相有功功率变量数据。 */
rt_err_t dlt645_read_active_power(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_ACTIVE);
}

/* 读取总、A、B、C相无功功率变量数据。 */
rt_err_t dlt645_read_reactive_power(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_REACTIVE);
}

/* 读取总、A、B、C相功率因数变量数据。 */
rt_err_t dlt645_read_power_factor(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_FACTOR);
}

/* 按规范顺序读取指定逆变器的全部变量数据。 */
rt_err_t dlt645_read_all_variables(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_variables(point, id, data, capacity, data_len, DLT645_VARIABLE_ALL);
}

/* 统一读取Pn或Qn，支持DI0选择单台及FF聚合全部12个固定档案槽位。 */
static rt_err_t dlt645_read_nominal_power(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                          uint16_t capacity, uint16_t *data_len, Dlt645NominalPowerTypeDef type)
{
    uint8_t selector = (uint8_t)id; /* 数据标识最低字节用于选择单台逆变器或全部逆变器。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要生成的固定长度Pn或Qn数据块数量。 */
    uint8_t archive_offset;         /* 当前处理相对首档案的槽位偏移。 */
    uint16_t required_len;          /* 单台或聚合读取所需的完整输出长度。 */
    uint16_t offset = 0U;           /* 当前已经写入输出缓冲区的字节数。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针只在统一入口检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由分发层验证后转换为槽位下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF固定返回12台，单台只返回一个数据块。 */
    required_len = point->data_len * archive_count; /* 点表data_len保存单台Pn或Qn的4字节长度。 */
    if(capacity < required_len) /* 缓冲区不足时禁止生成截断的645应答数据。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前读取的实际档案槽位下标。 */
        Inv_Data_t *inv = Inv_Data_Get(archive_index); /* 取得Pn和Qn的实时数据缓存。 */
        const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得Pn和Qn源数据的小数位配置。 */

        if((inv == RT_NULL) || (protocol == RT_NULL)) /* 下挂逆变器为空或未匹配协议时，当前4字节全部填FF。 */
        {
            rt_memset(&data[offset], 0xFF, point->data_len); /* 空档案仍保留固定位置，便于上位机按逻辑编号解析。 */
            offset += point->data_len;
            continue;
        }
        if(type == DLT645_NOMINAL_POWER_PN) /* Pn点使用实时Pn数据及协议Pn小数位。 */
        {
            dlt645_append_value(&inv->param.Pn, protocol->param.Pn.decimal_places, 4U, 4U, RT_FALSE, &data[offset]);
        }
        else /* Qn点使用实时Qn数据及协议Qn小数位。 */
        {
            dlt645_append_value(&inv->param.Qn, protocol->param.Qn.decimal_places, 4U, 4U, RT_FALSE, &data[offset]);
        }
        offset += point->data_len; /* 每台Pn或Qn固定占4字节，无效实时量也保持相同长度。 */
    }

    *data_len = offset; /* 全部槽位处理完成后返回本次实际业务数据长度。 */
    return (offset == required_len) ? RT_EOK : -RT_ERROR; /* 防止点表长度与编码实现意外不一致。 */
}

/* 读取指定逆变器或全部逆变器的额定有功功率Pn。 */
rt_err_t dlt645_read_Pn(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_nominal_power(point, id, data, capacity, data_len, DLT645_NOMINAL_POWER_PN);
}

/* 读取指定逆变器或全部逆变器的额定无功功率Qn。 */
rt_err_t dlt645_read_Qn(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    return dlt645_read_nominal_power(point, id, data, capacity, data_len, DLT645_NOMINAL_POWER_QN);
}

/* 读取逆变器输出类型，协议不支持、档案为空、数据无效或数值超出规范范围时返回FF。 */
rt_err_t dlt645_read_output_type(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                 uint16_t capacity, uint16_t *data_len)
{
    uint8_t selector = (uint8_t)id; /* 数据标识最低字节用于选择单台逆变器或全部逆变器。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要返回的输出类型字节数量。 */
    uint8_t archive_offset;         /* 当前处理相对首档案的槽位偏移。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针只在入口检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由分发层验证后转换为槽位下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF读取全部12个槽位，其他选择器读取单台。 */
    if(capacity < archive_count) /* 每台固定占1字节，缓冲区不足时禁止生成截断应答。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前读取的实际档案槽位下标。 */
        Inv_Data_t *inv = Inv_Data_Get(archive_index); /* 取得输出类型实时数据缓存。 */
        const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得输出类型寄存器支持信息。 */

        if((inv == RT_NULL) || (protocol == RT_NULL) ||
           (protocol->param.output_type.reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) ||
           (inv->param.output_type.valid == 0U) ||
           ((inv->param.output_type.value != 0) && (inv->param.output_type.value != 1))) /* 仅完整有效且符合00/01定义的数据可以返回。 */
        {
            data[archive_offset] = 0xFFU; /* 不支持或无有效数据时按规范返回FF。 */
            continue;
        }
        data[archive_offset] = (uint8_t)inv->param.output_type.value; /* 00表示单相，01表示三相。 */
    }

    *data_len = archive_count; /* 单台实际长度为1，全部逆变器实际长度为12。 */
    return RT_EOK;
}

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
