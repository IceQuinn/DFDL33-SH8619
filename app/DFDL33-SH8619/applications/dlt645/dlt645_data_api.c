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

/* 全量调节应答中每个字节表示对应档案槽位的最终处理状态。 */
typedef enum Dlt645ControlStatus
{
    DLT645_CONTROL_STATUS_SUCCESS = 0x00U,     /* 已向逆变器下发调节命令并收到成功响应。 */
    DLT645_CONTROL_STATUS_NOT_CONTROLLED = 0x01U, /* 写入值为全FF，因此没有执行下行调节。 */
    DLT645_CONTROL_STATUS_FAILED = 0x02U,      /* 下行发送、等待或设备响应阶段失败。 */
    DLT645_CONTROL_STATUS_UNSUPPORTED = 0x03U, /* 当前逆变器协议没有配置该调节功能。 */
    DLT645_CONTROL_STATUS_OTHER = 0x04U        /* 空档案写数值、数据非法或内部资源异常。 */
} Dlt645ControlStatusTypeDef;

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

/* 判断一个固定长度调节字段是否全部为FF，全FF表示主站明确要求该位置不控制。 */
static rt_bool_t dlt645_field_is_ff(const uint8_t *data, uint16_t data_len)
{
    uint16_t index; /* 当前检查的字段字节下标。 */

    for(index = 0U; index < data_len; ++index)
    {
        if(data[index] != 0xFFU) /* 任意字节不是FF都表示该字段携带了待解析数值。 */
        {
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

/* 将最高有效字节bit7为符号位的低字节在前BCD解码为有符号定点整数。 */
static rt_err_t dlt645_bcd_decode_s32(const uint8_t *data, uint16_t data_len, int32_t *value)
{
    uint8_t local_data[4]; /* 当前五类调节字段最长4字节，副本用于清除符号位后复用无符号解码。 */
    rt_bool_t negative;    /* 最高有效字节bit7置位表示负数。 */
    rt_err_t result;       /* 无符号BCD解码结果。 */

    if((data_len == 0U) || (data_len > sizeof(local_data))) /* 长度异常时不能访问最高有效字节。 */
    {
        return -RT_EINVAL;
    }
    rt_memcpy(local_data, data, data_len); /* 不修改解析层提供的原始写数据。 */
    negative = ((local_data[data_len - 1U] & 0x80U) != 0U) ? RT_TRUE : RT_FALSE; /* 提取符号后再检查BCD半字节。 */
    local_data[data_len - 1U] &= 0x7FU; /* bit7不参与最高十进制数字的BCD合法性检查。 */
    result = dlt645_bcd_decode_u32(local_data, data_len, value); /* 绝对值沿用统一的低字节在前BCD解码。 */
    if(result != RT_EOK)
    {
        return result;
    }
    if(negative == RT_TRUE) /* 负零也统一归一化为整数0，避免生成无意义的负零控制。 */
    {
        *value = -*value;
    }
    return RT_EOK;
}

/* 根据数据标识取得五类调节量对应的控制类型、协议配置和实时缓存。 */
static rt_err_t dlt645_get_control_source(uint32_t id,
                                          uint8_t archive_index,
                                          Inv_Control_Type_t *control_type,
                                          const Inv_CtrlRegBlk_t **control_reg,
                                          Inv_RealtimeValue_t **source)
{
    const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得该档案绑定的厂家协议配置。 */
    Inv_Data_t *inv_data = Inv_Data_Get(archive_index); /* 取得与档案槽位对应的控制值缓存。 */
    uint8_t point_type = (uint8_t)(id >> 8); /* DI1的05～09区分五类调节量。 */

    if(inv_data == RT_NULL) /* 空档案由调用方区分FF跳过和数值错误。 */
    {
        return -RT_EEMPTY;
    }
    if(protocol == RT_NULL) /* 有效档案没有匹配协议时按不支持处理。 */
    {
        return -RT_ENOSYS;
    }

    switch(point_type)
    {
    case 0x05U:
        *control_type = INV_CONTROL_ACTIVE_POWER;
        *control_reg = &protocol->ctrl.active_pwr_ctrl;
        *source = &inv_data->ctrl.active_pwr_ctrl;
        break;
    case 0x06U:
        *control_type = INV_CONTROL_REACTIVE_POWER;
        *control_reg = &protocol->ctrl.reactive_pwr_ctrl;
        *source = &inv_data->ctrl.reactive_pwr_ctrl;
        break;
    case 0x07U:
        *control_type = INV_CONTROL_POWER_FACTOR;
        *control_reg = &protocol->ctrl.pwr_factor_ctrl;
        *source = &inv_data->ctrl.pwr_factor_ctrl;
        break;
    case 0x08U:
        *control_type = INV_CONTROL_ACTIVE_POWER_PERCENT;
        *control_reg = &protocol->ctrl.active_pwr_pct_ctrl;
        *source = &inv_data->ctrl.active_pwr_pct_ctrl;
        break;
    case 0x09U:
        *control_type = INV_CONTROL_REACTIVE_POWER_PERCENT;
        *control_reg = &protocol->ctrl.reactive_pwr_pct_ctrl;
        *source = &inv_data->ctrl.reactive_pwr_pct_ctrl;
        break;
    default:
        return -RT_EINVAL; /* 点表外的数据标识不能映射到逆变器控制配置。 */
    }
    if((*control_reg)->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) /* 未配置寄存器地址表示该厂家协议不支持此功能。 */
    {
        return -RT_ENOSYS;
    }
    return RT_EOK;
}

/* 把点表倍率转换为十进制小数位，五类点仅允许倍率1、10、100、1000或10000。 */
static rt_err_t dlt645_point_decimal_places(const Dlt645PointTypeDef *point, uint8_t *decimal_places)
{
    int32_t scale = point->scale; /* 使用局部变量逐次除10，避免修改静态点表。 */
    uint8_t decimals = 0U;        /* 当前倍率对应的小数位数。 */

    if(scale < 1) /* 非正倍率不能表示十进制定点数。 */
    {
        return -RT_EINVAL;
    }
    while(scale > 1)
    {
        if((scale % 10) != 0) /* 不是10的整数次幂时无法与协议decimal_places精确换算。 */
        {
            return -RT_EINVAL;
        }
        scale /= 10;
        ++decimals;
    }
    *decimal_places = decimals; /* 完整验证倍率后再输出小数位。 */
    return RT_EOK;
}

/* 读取单台或全部逆变器的五类调节设定值，固定槽位无数据时返回全FF。 */
rt_err_t dlt645_read_control_value(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                   uint16_t capacity, uint16_t *data_len)
{
    uint8_t selector = (uint8_t)id; /* DI0选择单台档案或全部12个档案。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次读取的固定槽位数量。 */
    uint8_t archive_offset;         /* 当前处理的相对槽位下标。 */
    uint8_t target_decimals;        /* 645点表倍率对应的小数位数。 */
    uint16_t required_len;          /* 本次应答需要生成的业务数据长度。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针只在入口检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* 选择器已由分发层完成范围检查。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF固定返回12个位置。 */
    required_len = point->data_len * archive_count; /* 每台字段长度由当前点表定义。 */
    if((capacity < required_len) || (dlt645_point_decimal_places(point, &target_decimals) != RT_EOK)) /* 缓冲区和倍率必须有效。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        Inv_Control_Type_t control_type;       /* 当前数据标识对应的下行控制类型。 */
        const Inv_CtrlRegBlk_t *control_reg;   /* 当前逆变器协议中的控制寄存器配置。 */
        Inv_RealtimeValue_t *source;           /* 当前逆变器最近一次有效控制回读值。 */
        uint8_t *field = &data[archive_offset * point->data_len]; /* 当前档案在应答中的固定字段位置。 */

        if(dlt645_get_control_source(id, first_archive + archive_offset, &control_type, &control_reg, &source) != RT_EOK) /* 空档案或不支持时保留槽位并填FF。 */
        {
            rt_memset(field, 0xFF, point->data_len);
            continue;
        }
        RT_UNUSED(control_type); /* 读取只需要配置和缓存，控制类型由公共映射接口同时返回供写入复用。 */
        dlt645_append_value(source, control_reg->decimal_places, target_decimals, (uint8_t)point->data_len, RT_TRUE, field); /* 五类调节值统一使用符号位BCD。 */
    }
    *data_len = required_len; /* 单台返回一个字段，全量返回12个固定字段。 */
    return RT_EOK;
}

/* 将逆变器控制模块的执行结果映射为规范定义的逐台645状态。 */
static uint8_t dlt645_control_result_to_status(Inv_Control_Result_t result)
{
    if(result == INV_CONTROL_RESULT_OK) /* 完整收到合法Modbus写响应才表示调控成功。 */
    {
        return DLT645_CONTROL_STATUS_SUCCESS;
    }
    if((result == INV_CONTROL_RESULT_UNSUPPORTED) || (result == INV_CONTROL_RESULT_PROTOCOL_MISSING)) /* 缺少对应厂家控制配置归入不支持。 */
    {
        return DLT645_CONTROL_STATUS_UNSUPPORTED;
    }
    if((result == INV_CONTROL_RESULT_SEND_FAILED) || (result == INV_CONTROL_RESULT_TIMEOUT) ||
       (result == INV_CONTROL_RESULT_RESPONSE_INVALID) || (result == INV_CONTROL_RESULT_DEVICE_EXCEPTION)) /* 已尝试控制但通信或设备响应失败。 */
    {
        return DLT645_CONTROL_STATUS_FAILED;
    }
    return DLT645_CONTROL_STATUS_OTHER; /* 档案变化、转换失败等剩余内部问题统一返回04。 */
}

/* 提交一台数值调节请求并按唯一流水号等待最终结果。 */
static uint8_t dlt645_control_one_value(uint8_t archive_index, Inv_Control_Type_t control_type, int32_t value)
{
    Inv_Control_Request_t request;    /* 即将提交到档案所属端口的数值调节请求。 */
    Inv_Control_Result_Info_t result; /* 与本次唯一流水号对应的异步执行结果。 */
    rt_err_t control_result;          /* 提交或等待公共控制接口的返回码。 */

    request.request_id = Inv_Control_Allocate_Request_Id(); /* 所有控制来源共用唯一流水号，防止取错异步结果。 */
    request.archive_index = archive_index; /* 保存当前固定档案槽位。 */
    request.value = value; /* 数值已经换算为目标逆变器协议的小数位。 */
    request.type = control_type; /* 保存本次需要使用的控制寄存器类型。 */
    control_result = Inv_Control_Submit(&request); /* 将请求放入档案所属下行端口队列。 */
    if(control_result != RT_EOK) /* 队列满或内部状态异常时没有异步结果可等待。 */
    {
        return DLT645_CONTROL_STATUS_OTHER;
    }
    control_result = Inv_Control_Get_Result_By_Id(request.request_id, &result, 1000); /* 暂按现有要求最多等待1000个系统tick。 */
    if(control_result != RT_EOK) /* 等待不到对应请求结果属于实际调控失败。 */
    {
        return DLT645_CONTROL_STATUS_FAILED;
    }
    return dlt645_control_result_to_status(result.result); /* 将下行详细结果压缩为00～04状态。 */
}

/* 写入单台或全部逆变器的五类调节值，全量写入时逐个返回对应处理状态。 */
rt_err_t dlt645_write_control_value(const Dlt645PointTypeDef *point, uint32_t id, const uint8_t *data,
                                    uint16_t data_len, uint8_t *response,
                                    uint16_t response_capacity, uint16_t *response_len)
{
    uint8_t selector = (uint8_t)id; /* DI0选择单台档案或全部12个档案。 */
    uint8_t first_archive;          /* 本次写入的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次写入包含的固定槽位数量。 */
    uint8_t archive_offset;         /* 当前预检查或执行的相对槽位下标。 */
    uint8_t source_decimals;        /* 645点表数值对应的小数位数。 */
    int32_t values[INVERTER_ARCHIVE_MAX_COUNT]; /* 完成BCD解码并换算到逆变器协议精度的控制值。 */
    Inv_Control_Type_t types[INVERTER_ARCHIVE_MAX_COUNT]; /* 每个有效数值对应的下行控制类型。 */
    rt_bool_t execute[INVERTER_ARCHIVE_MAX_COUNT]; /* 真表示当前槽位通过检查并需要实际下发。 */
    rt_bool_t has_numeric_value = RT_FALSE; /* 用于区分全FF占位请求和实际控制请求。 */

    if((point == RT_NULL) || (data == RT_NULL) || (response == RT_NULL) || (response_len == RT_NULL)) /* 写入口的重要指针只检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* 选择器已由上层验证。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF写入必须提供12个一一对应的值。 */
    if((data_len != (uint16_t)(point->data_len * archive_count)) ||
       (response_capacity < archive_count) ||
       (dlt645_point_decimal_places(point, &source_decimals) != RT_EOK)) /* 长度、结果缓存和点表倍率必须完整有效。 */
    {
        return -RT_EINVAL;
    }
    rt_memset(execute, 0, sizeof(execute)); /* 默认所有槽位都不执行，只有完整通过检查后才置位。 */
    rt_memset(response, DLT645_CONTROL_STATUS_OTHER, archive_count); /* 尚未分类的槽位默认返回04其他问题。 */

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        const uint8_t *field = &data[archive_offset * point->data_len]; /* 当前逆变器对应的完整写字段。 */
        const Inv_CtrlRegBlk_t *control_reg; /* 当前档案对应调节寄存器的小数位和支持信息。 */
        Inv_RealtimeValue_t *source;         /* 映射接口要求返回的缓存指针，本次写预检查不直接使用。 */
        int32_t decoded_value;               /* 从符号位BCD解码得到的645定点整数。 */
        rt_err_t source_result;              /* 档案、协议和控制配置查询结果。 */

        if(dlt645_field_is_ff(field, point->data_len) == RT_TRUE) /* 全FF明确表示该槽位不控制，不检查档案是否存在。 */
        {
            response[archive_offset] = DLT645_CONTROL_STATUS_NOT_CONTROLLED;
            continue;
        }
        has_numeric_value = RT_TRUE; /* 至少一个数值意味着本帧包含实际控制意图。 */
        source_result = dlt645_get_control_source(id, first_archive + archive_offset, &types[archive_offset], &control_reg, &source);
        if(source_result == -RT_ENOSYS) /* 有效档案没有对应控制配置时返回03。 */
        {
            response[archive_offset] = DLT645_CONTROL_STATUS_UNSUPPORTED;
            continue;
        }
        if(source_result != RT_EOK) /* 空档案写数值或其他映射错误统一返回04。 */
        {
            continue;
        }
        RT_UNUSED(source); /* 写入只使用控制配置，实时缓存由控制状态机在回读成功后更新。 */
        if((dlt645_bcd_decode_s32(field, point->data_len, &decoded_value) != RT_EOK) ||
           (decoded_value < point->write_min) || (decoded_value > point->write_max) ||
           (dlt645_rescale_value(decoded_value, source_decimals, control_reg->decimal_places,
                                 &values[archive_offset]) != RT_EOK)) /* BCD、业务范围和协议定点换算任一失败均返回04。 */
        {
            continue;
        }
        execute[archive_offset] = RT_TRUE; /* 完整通过预检查后才允许进入实际下行控制。 */
    }

    if((has_numeric_value == RT_TRUE) && (Inv_Control_Is_Work_Enabled() != RT_TRUE)) /* 非工作时段只要包含数值就整帧拒绝且不执行任何设备。 */
    {
        return -RT_EBUSY;
    }
    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        if(execute[archive_offset] == RT_TRUE) /* FF、空档案、非法数值和不支持项都不会进入下行队列。 */
        {
            response[archive_offset] = dlt645_control_one_value(first_archive + archive_offset,
                                                                 types[archive_offset], values[archive_offset]);
        }
    }
    *response_len = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? archive_count : 0U; /* 仅全量控制在正常写应答中携带逐台状态。 */
    if(selector != DLT645_VARIABLE_ALL_SELECTOR) /* 单台写没有逐台状态数据域，需要通过645正常或异常应答表达结果。 */
    {
        return ((response[0] == DLT645_CONTROL_STATUS_SUCCESS) ||
                (response[0] == DLT645_CONTROL_STATUS_NOT_CONTROLLED)) ? RT_EOK : -RT_ERROR; /* 成功或FF不控制正常回复，其余状态返回写异常。 */
    }
    return RT_EOK; /* 全量请求始终通过12个状态表达各槽位结果。 */
}

/* 读取DI0指定逆变器或全部逆变器的推导运行状态，空档案和未知状态使用FF占位。 */
rt_err_t dlt645_read_run_state(const Dlt645PointTypeDef *point,
                               uint32_t id,
                               uint8_t *data,
                               uint16_t capacity,
                               uint16_t *data_len)
{
    uint8_t selector = (uint8_t)id; /* 数据标识最低字节用于选择单台逆变器或全部逆变器。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要返回的运行状态字节数量。 */
    uint8_t archive_offset;         /* 当前处理相对首档案的槽位偏移。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针只在入口检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由分发层验证后转换为槽位下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF返回全部12台，普通DI0只返回单台。 */
    if(capacity < archive_count) /* 每台运行状态固定占1字节，缓冲区不足时禁止截断返回。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        Inv_Data_t *inv_data = Inv_Data_Get(first_archive + archive_offset); /* 通过公共接口取得当前档案实时数据。 */

        if((inv_data == RT_NULL) || (inv_data->run_state == INV_RUN_STATE_UNKNOWN)) /* 空档案或无法推导状态时返回FF。 */
        {
            data[archive_offset] = 0xFFU;
        }
        else if(inv_data->run_state == INV_RUN_STATE_ON) /* 规范定义00表示开机。 */
        {
            data[archive_offset] = 0x00U;
        }
        else /* 剩余有效枚举为关机状态，规范编码为01。 */
        {
            data[archive_offset] = 0x01U;
        }
    }
    *data_len = archive_count; /* 单台返回1字节，DI0为FF时返回12字节。 */
    return RT_EOK;
}

/* 提交一台逆变器的开关机请求并按唯一流水号等待其最终结果。 */
static rt_err_t dlt645_control_run_state(uint8_t archive_index, int32_t value)
{
    Inv_Control_Request_t request;    /* 即将提交到目标档案所在端口的控制请求。 */
    Inv_Control_Result_Info_t result; /* 与本次唯一流水号对应的最终控制结果。 */
    rt_err_t control_result;          /* 控制提交或等待结果接口的返回码。 */

    request.request_id = Inv_Control_Allocate_Request_Id(); /* 使用公共分配器避免与其他控制来源发生流水号冲突。 */
    request.archive_index = archive_index; /* 保存当前需要控制的固定档案槽位。 */
    request.value = value; /* 保存645状态值，便于控制结果日志关联。 */
    request.type = (value == 0) ? INV_CONTROL_POWER_ON : INV_CONTROL_POWER_OFF; /* 0执行开机，1执行关机。 */

    control_result = Inv_Control_Submit(&request); /* 将请求提交到档案所属下行端口。 */
    if(control_result != RT_EOK) /* 无效档案、非工作时段开机或队列不可用时直接失败。 */
    {
        return control_result;
    }
    control_result = Inv_Control_Get_Result_By_Id(request.request_id, &result, 1000); /* 暂按现有要求最多等待1000个系统tick。 */
    if((control_result != RT_EOK) || (result.result != INV_CONTROL_RESULT_OK)) /* 超时或设备最终控制失败均返回错误。 */
    {
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* 解码并校验单台或12台运行状态写数据，全部目标控制成功后才返回成功。 */
rt_err_t dlt645_write_run_state(const Dlt645PointTypeDef *point,
                                uint32_t id,
                                const uint8_t *data,
                                uint16_t data_len,
                                uint8_t *response,
                                uint16_t response_capacity,
                                uint16_t *response_len)
{
    uint8_t selector = (uint8_t)id; /* DI0等于FF时数据内包含12台控制值，否则只包含单台控制值。 */
    uint8_t first_archive;          /* 本次控制的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要逐个执行的控制数量。 */
    uint8_t archive_offset;         /* 当前校验或控制相对首档案的偏移。 */
    int32_t values[INVERTER_ARCHIVE_MAX_COUNT]; /* 先完整校验后保存的12个状态值，避免格式错误时产生部分控制。 */
    rt_bool_t all_success = RT_TRUE; /* 只有每个档案最终控制成功时才保持为真。 */

    RT_UNUSED(response); /* 运行状态沿用原有无数据写应答，不生成逐台状态数据域。 */
    RT_UNUSED(response_capacity);
    *response_len = 0U; /* 明确通知顶层使用原有正常或异常状态应答。 */

    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由上层验证后转换为槽位下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF要求逐个控制全部12个档案。 */
    if(data_len != (uint16_t)(point->data_len * archive_count)) /* 单台必须1字节，全部控制必须包含12字节。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        if((dlt645_bcd_decode_u32(&data[archive_offset], point->data_len, &values[archive_offset]) != RT_EOK) ||
           (values[archive_offset] < point->write_min) ||
           (values[archive_offset] > point->write_max)) /* 12个值必须全部为合法BCD且只能取0或1。 */
        {
            return -RT_EINVAL;
        }
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        if(dlt645_control_run_state(first_archive + archive_offset, values[archive_offset]) != RT_EOK) /* 无效档案或控制失败会使最终645应答为错误。 */
        {
            all_success = RT_FALSE;
        }
    }
    return (all_success == RT_TRUE) ? RT_EOK : -RT_ERROR; /* 仅全部12台或目标单台成功时正常应答。 */
}
