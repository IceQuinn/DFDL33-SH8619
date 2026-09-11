#include "dlt645_data_api.h"

#include <stdint.h>
#include <rtthread.h>

#include "inv_data.h"
#include "inverter_protocol_library.h"
#include "ctu_cfg.h"
#include "event_deal.h"
#include "sys.h"
#include "time_ctrl.h"
#include "user_iwdg.h"
#include "user_rtc.h"
#include "voltage_acq.h"

#define DLT645_VARIABLE_ALL_SELECTOR  0xFFU /* DI0等于FF时按档案顺序返回全部12台逆变器。 */
#define DLT645_VOLTAGE_LEN            6U    /* 三相电压各占2字节，单台合计6字节。 */
#define DLT645_CURRENT_LEN            9U    /* 三相电流各占3字节，单台合计9字节。 */
#define DLT645_POWER_LEN              16U   /* 总、A、B、C相功率各占4字节，单台合计16字节。 */
#define DLT645_POWER_FACTOR_LEN       8U    /* 总、A、B、C相功率因数各占2字节，单台合计8字节。 */
#define DLT645_ALL_VARIABLE_LEN       55U   /* 五组变量按规范顺序拼接后的单台总长度。 */
#define DLT645_ARCHIVE_ADDRESS_OFFSET 0U    /* 档案数据块第1字节为Modbus地址。 */
#define DLT645_ARCHIVE_NAME_OFFSET    1U    /* Modbus地址之后为固定32字节厂家名称。 */
#define DLT645_ARCHIVE_VERSION_OFFSET 33U   /* 厂家名称之后为低字节在前的16位规约版本。 */
#define DLT645_ARCHIVE_PORT_OFFSET    35U   /* 档案数据块最后1字节为接入端口号。 */
#define DLT645_CONVERTER_VOLTAGE_DEFAULT 2200U /* 临时A相电压为220.0V，内存单位固定为0.1V。 */
#define DLT645_LOCATION_DATA_LEN       11U      /* 位置信息由4字节经度、4字节纬度和3字节高度组成。 */
#define DLT645_LONGITUDE_MAX           1800000U /* 经度格式XXXX.XXXX，业务有效范围限制为0～180.0000度。 */
#define DLT645_LATITUDE_MAX             900000U /* 纬度格式XXXX.XXXX，业务有效范围限制为0～90.0000度。 */
#define DLT645_ALTITUDE_MAX             999999U /* 高度格式XXXX.XX，三字节BCD最大表示9999.99米。 */
#define DLT645_SERIAL_PARAMETER_LEN          5U /* 串口参数由4字节小端波特率和1字节校验格式组成。 */
#define DLT645_POLL_INTERVAL_LEN             2U /* 5～3600秒需要两字节低字节在前BCD才能完整表示。 */
#define DLT645_POLL_INTERVAL_MIN             5U /* 全局周期抄读间隔最小允许5秒。 */
#define DLT645_POLL_INTERVAL_MAX          3600U /* 全局周期抄读间隔最大允许3600秒。 */
#define DLT645_FIRMWARE_VERSION_LEN         32U /* 04800001厂家软件版本号固定占32字节ASCII。 */

/* 读取RAM协议库的当前有效数量；协议库数量使用原始整数而不是BCD编码。 */
rt_err_t dlt645_read_protocol_count(const Dlt645PointTypeDef *point, uint32_t id,
                                    uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    RT_UNUSED(id); /* 04E701FE是固定数据标识，不使用DI0作为槽位下标。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) || (point->data_len != 1U) || (capacity < 1U)) /* 入口只执行一次必要的参数检查。 */
    {
        return -RT_EINVAL;
    }
    data[0] = Inv_Proto_Valid_Count(); /* 有效数量随临时协议写入实时变化，范围固定为0～100。 */
    *data_len = 1U;
    return RT_EOK;
}

/* 读取DI0指定的协议槽位，协议模块负责对无效槽位填充238字节FF。 */
rt_err_t dlt645_read_protocol(const Dlt645PointTypeDef *point, uint32_t id,
                              uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint16_t proto_number = (uint8_t)id; /* 分发层已将DI0限制为01～64，可直接作为1起始协议编号。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) || (point->data_len != INV_PROTO_SIZE) || (capacity < INV_PROTO_SIZE)) /* 完整协议必须一次容纳238字节。 */
    {
        return -RT_EINVAL;
    }
    if(Inv_Proto_Read_Wire(proto_number, data, capacity) != RT_EOK) /* 槽位读取失败时由645顶层返回无请求数据异常。 */
    {
        return -RT_ERROR;
    }
    *data_len = INV_PROTO_SIZE;
    return RT_EOK;
}

/* 校验并临时写入DI0指定协议槽位，标准写应答不携带额外业务数据。 */
rt_err_t dlt645_write_protocol(const Dlt645PointTypeDef *point, uint32_t id,
                               const uint8_t *data, uint16_t data_len,
                               uint8_t *response, uint16_t response_capacity,
                               uint16_t *response_len)
{
    RT_UNUSED(response); /* 协议库写入成功使用645标准无数据写应答。 */
    RT_UNUSED(response_capacity);
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) || (point->data_len != INV_PROTO_SIZE) || (data_len != INV_PROTO_SIZE)) /* 写请求必须完整携带单条协议。 */
    {
        return -RT_EINVAL;
    }
    *response_len = 0U; /* 明确禁止沿用上一次带状态写应答的数据长度。 */
    return Inv_Proto_Write_Wire((uint8_t)id, data, data_len); /* 成功后周期抄读立即使用该槽位的新内容，不执行Flash保存。 */
}

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

/* 获取协议转换单元A相电压，当前返回0.1V单位的默认值，后续在本函数内接入真实采样接口。 */
uint16_t dlt645_get_converter_phase_a_voltage(void)
{
    return getvoltage_rms();
//    return DLT645_CONVERTER_VOLTAGE_DEFAULT; /* 2200表示220.0V，调用方不需要再执行浮点换算。 */
}

/* 读取协议转换单元A相电压，02010100和06100101共用本接口及同一数据来源。 */
rt_err_t dlt645_read_converter_phase_a_voltage(const Dlt645PointTypeDef *point, uint32_t id,
                                               uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint16_t voltage = dlt645_get_converter_phase_a_voltage(); /* 取得单位为0.1V的协议转换单元A相电压。 */

    RT_UNUSED(id); /* 两个固定数据标识的数据格式相同，不需要在取值接口内区分。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != 2U) || (capacity < point->data_len)) /* 指针、点表长度和输出容量必须满足XXX.X两字节BCD要求。 */
    {
        return -RT_EINVAL;
    }
    if((voltage > 9999U) || (dlt645_encode_bcd(voltage, data, 2U, RT_FALSE) != RT_EOK)) /* XXX.X格式最多表示999.9V，越界值不能截断回复。 */
    {
        return -RT_EINVAL;
    }

    *data_len = point->data_len; /* 02010100和06100101均固定返回两字节业务数据。 */
    return RT_EOK;
}

/* 根据点表长度生成全零业务数据，供当前规范中没有实际数据来源的其他类数据标识复用。 */
rt_err_t dlt645_read_zero_data(const Dlt645PointTypeDef *point, uint32_t id,
                               uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    RT_UNUSED(id); /* 回零点不根据数据标识计算数值，长度完全由已经匹配的点表描述决定。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len == 0U) || (capacity < point->data_len)) /* 输出指针有效且缓冲区能够容纳点表规定长度时才允许填零。 */
    {
        return -RT_EINVAL;
    }

    rt_memset(data, 0, point->data_len); /* 这里生成减去0x33后的全零业务数据，顶层组帧时再统一加0x33。 */
    *data_len = point->data_len; /* 返回点表规定的固定业务数据长度，避免不同标识之间长度混用。 */
    return RT_EOK;
}

/* 读取04800001厂家软件版本号，将启动时生成的版本字符串组织为固定32字节零填充ASCII。 */
rt_err_t dlt645_read_firmware_version(const Dlt645PointTypeDef *point, uint32_t id,
                                      uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint16_t index; /* 当前复制的版本字符串字节下标，最大不超过协议规定的32字节。 */

    RT_UNUSED(id); /* 04800001是固定数据标识，不使用DI0选择档案或数据块。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != DLT645_FIRMWARE_VERSION_LEN) || (capacity < point->data_len)) /* 一次检查接口边界和固定32字节输出容量。 */
    {
        return -RT_EINVAL;
    }

    rt_memset(data, 0, point->data_len); /* 字符串有效内容之后统一补0x00，避免把静态缓冲区旧数据带入应答。 */
    for(index = 0U; index < point->data_len; ++index) /* 有界复制避免未来版本格式变化时越过32字节协议字段。 */
    {
        if(app_show_ver_ascll[index] == '\0') /* 遇到C字符串结束符后保留已经填好的零字节尾部。 */
        {
            break;
        }
        data[index] = (uint8_t)app_show_ver_ascll[index]; /* 版本号中的数字、点、空格和日期符号按原始ASCII上送。 */
    }

    *data_len = point->data_len; /* 无论实际字符串多长，04800001都固定返回32字节业务数据。 */
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

/* 读取当前有效光伏逆变器档案数量，并按规范编码为一字节低位BCD。 */
rt_err_t dlt645_read_archive_count(const Dlt645PointTypeDef *point, uint32_t id,
                                   uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint8_t archive_count = g_inv_archive_lib.count; /* 档案保存和初始化流程已经根据valid数组维护有效档案数量。 */

    (void)id; /* 本接口仅处理固定数据标识04E62100，不需要解析设备选择器。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (capacity < point->data_len) || (point->data_len != 1U)) /* 首次接收的接口参数和点表长度必须满足单字节输出要求。 */
    {
        return -RT_EINVAL;
    }
    if(archive_count > INVERTER_ARCHIVE_MAX_COUNT) /* 持久化数量异常时禁止向主站返回超出规范范围的数据。 */
    {
        return -RT_EINVAL;
    }
    if(dlt645_encode_bcd(archive_count, data, 1U, RT_FALSE) != RT_EOK) /* 0～12必须能够编码成规范要求的NN格式。 */
    {
        return -RT_EINVAL;
    }

    *data_len = 1U; /* 光伏逆变器档案数量固定占一个BCD字节。 */
    return RT_EOK;
}

/* 判断完整档案数据块是否全部为FF，全FF档案当前不用于删除并应作为非法写入处理。 */
static rt_bool_t dlt645_archive_is_all_ff(const uint8_t *data)
{
    uint16_t index; /* 当前检查的档案数据块字节下标。 */

    for(index = 0U; index < INVERTER_ARCHIVE_WIRE_SIZE; ++index)
    {
        if(data[index] != 0xFFU) /* 任一字节不是FF即表示主站提交了具体档案内容。 */
        {
            return RT_FALSE;
        }
    }
    return RT_TRUE;
}

/* 将主站下发的固定32字节厂家字段规范化为协议库使用的零填充ASCII名称。 */
static rt_err_t dlt645_archive_normalize_name(const uint8_t *source,
                                              char target[INVERTER_ARCHIVE_BRAND_WIRE_SIZE])
{
    uint8_t source_len = INVERTER_ARCHIVE_BRAND_WIRE_SIZE; /* 首个结束符之前的厂家名称长度。 */
    uint8_t index;                                         /* 厂家字段扫描和复制下标。 */

    for(index = 0U; index < INVERTER_ARCHIVE_BRAND_WIRE_SIZE; ++index)
    {
        if((source[index] == 0x00U) || (source[index] == 0xFFU)) /* 00和FF均可作为固定长度厂家字符串的结束填充值。 */
        {
            source_len = index;
            break;
        }
        if((source[index] < 0x20U) || (source[index] > 0x7EU)) /* 厂家名称只接受可打印ASCII字符。 */
        {
            return -RT_EINVAL;
        }
    }
    while((source_len > 0U) && (source[source_len - 1U] == 0x20U)) /* 去除上位机可能补在名称末尾的空格。 */
    {
        --source_len;
    }
    if(source_len == 0U) /* 空厂家名称无法与有效协议库建立明确对应关系。 */
    {
        return -RT_EINVAL;
    }
    for(index = source_len; index < INVERTER_ARCHIVE_BRAND_WIRE_SIZE; ++index)
    {
        if((source[index] != 0x00U) && (source[index] != 0xFFU) && (source[index] != 0x20U)) /* 名称结束后只允许00、FF或空格填充。 */
        {
            return -RT_EINVAL;
        }
    }

    rt_memset(target, 0, INVERTER_ARCHIVE_BRAND_WIRE_SIZE); /* 协议库厂家名称统一使用零填充，保证固定长度比较稳定。 */
    rt_memcpy(target, source, source_len); /* 只复制规范化后的有效ASCII名称，不保留主站填充字节。 */
    return RT_EOK;
}

/* 读取DI0指定的固定档案槽位，有效档案按36字节字段顺序输出，无效档案输出全FF。 */
rt_err_t dlt645_read_archive(const Dlt645PointTypeDef *point, uint32_t id,
                             uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint8_t archive_index = (uint8_t)id - 1U; /* 分发层已确认DI0为01～0C，此处转换为0～11槽位下标。 */
    const Inv_Archive_t *archive;             /* 当前请求档案槽位的只读结构地址。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != INVERTER_ARCHIVE_WIRE_SIZE) || (capacity < point->data_len)) /* 接口参数和点表长度必须能够容纳完整档案。 */
    {
        return -RT_EINVAL;
    }
    if(g_inv_archive_lib.valid[archive_index] != INVERTER_ARCHIVE_VALID) /* 无效槽位按规范返回固定36字节全FF。 */
    {
        rt_memset(data, 0xFF, INVERTER_ARCHIVE_WIRE_SIZE);
        *data_len = INVERTER_ARCHIVE_WIRE_SIZE;
        return RT_EOK;
    }

    archive = &g_inv_archive_lib.archives[archive_index]; /* 有效标志确认后再取得对应档案内容。 */
    data[DLT645_ARCHIVE_ADDRESS_OFFSET] = archive->mb_addr; /* Modbus地址按原始无符号字节传输。 */
    rt_memcpy(&data[DLT645_ARCHIVE_NAME_OFFSET], archive->mfr_info.name, INVERTER_ARCHIVE_BRAND_WIRE_SIZE); /* 厂家名称固定输出32字节ASCII。 */
    data[DLT645_ARCHIVE_VERSION_OFFSET] = (uint8_t)archive->mfr_info.proto_ver; /* 规约版本先发送uint16_t低字节。 */
    data[DLT645_ARCHIVE_VERSION_OFFSET + 1U] = (uint8_t)(archive->mfr_info.proto_ver >> 8U); /* 规约版本随后发送uint16_t高字节。 */
    data[DLT645_ARCHIVE_PORT_OFFSET] = archive->port; /* 端口号按规范定义的01～04原值传输。 */
    *data_len = INVERTER_ARCHIVE_WIRE_SIZE; /* 单条逆变器档案固定返回36字节。 */
    return RT_EOK;
}

/* 解析并写入DI0指定的逆变器档案，只有厂家和规约版本匹配有效协议库时才保存。 */
rt_err_t dlt645_write_archive(const Dlt645PointTypeDef *point, uint32_t id,
                              const uint8_t *data, uint16_t data_len,
                              uint8_t *response, uint16_t response_capacity,
                              uint16_t *response_len)
{
    uint8_t archive_index = (uint8_t)id - 1U; /* 分发层已确认DI0为01～0C，此处定位主站指定的档案槽位。 */
    Inv_Archive_t archive;                    /* 完成全部字段校验后提交给档案模块的临时档案。 */

    RT_UNUSED(response); /* 单档案写入成功使用标准无数据写应答，不生成附加状态数据。 */
    RT_UNUSED(response_capacity); /* 不生成附加状态数据，因此无需占用应答数据缓冲区。 */
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) ||
       (point->data_len != INVERTER_ARCHIVE_WIRE_SIZE) || (data_len != point->data_len)) /* 指针和业务数据长度必须满足完整36字节档案要求。 */
    {
        return -RT_EINVAL;
    }
    if(dlt645_archive_is_all_ff(data) == RT_TRUE) /* 当前版本不支持使用全FF写请求删除档案。 */
    {
        return -RT_EINVAL;
    }

    rt_memset(&archive, 0, sizeof(archive)); /* 清除结构体填充内容，厂家名称也以零作为统一填充值。 */
    archive.mb_addr = data[DLT645_ARCHIVE_ADDRESS_OFFSET]; /* Modbus地址按原始1字节读取，合法范围由档案模块统一校验。 */
    if(dlt645_archive_normalize_name(&data[DLT645_ARCHIVE_NAME_OFFSET], archive.mfr_info.name) != RT_EOK) /* 厂家字段必须是非空、可打印并可规范化的ASCII字符串。 */
    {
        return -RT_EINVAL;
    }
    archive.mfr_info.proto_ver = (uint16_t)data[DLT645_ARCHIVE_VERSION_OFFSET] |
                                 ((uint16_t)data[DLT645_ARCHIVE_VERSION_OFFSET + 1U] << 8U); /* 两个线上字节按小端顺序还原为0x0100等规约版本值。 */
    archive.port = data[DLT645_ARCHIVE_PORT_OFFSET]; /* 端口号01～04由档案模块按规范枚举统一校验。 */

    if(Inv_Archive_Set(archive_index, &archive) == INVERTER_ARCHIVE_ADD_FAILED) /* 地址端口非法、设备重复或厂家规约未匹配时保持原档案不变。 */
    {
        return -RT_EINVAL;
    }

    *response_len = 0U; /* 写入成功后返回标准645正常写应答，不携带业务数据。 */
    return RT_EOK;
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

/* 读取单台或全部逆变器日发电量，空档案、不支持、数据无效或编码溢出时返回全FF。 */
rt_err_t dlt645_read_daily_energy(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                  uint16_t capacity, uint16_t *data_len)
{
    uint8_t selector = (uint8_t)id; /* 数据标识最低字节用于选择单台逆变器或全部逆变器。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要返回的固定日发电量字段数量。 */
    uint8_t archive_offset;         /* 当前处理相对首档案的槽位偏移。 */
    uint16_t required_len;          /* 单台或聚合读取对应的完整业务数据长度。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共接口的重要指针只在统一入口检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* DI0已由分发层验证后转换为档案下标。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF固定返回全部12个档案槽位。 */
    required_len = point->data_len * archive_count; /* 每台日发电量固定占4字节。 */
    if(capacity < required_len) /* 输出缓冲区不足时禁止生成截断的645应答。 */
    {
        return -RT_EINVAL;
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前读取的实际档案槽位下标。 */
        uint8_t *field = &data[archive_offset * point->data_len]; /* 当前档案在固定应答数据块中的字段地址。 */
        Inv_Data_t *inv = Inv_Data_Get(archive_index); /* 取得周期抄读维护的日发电量实时缓存。 */
        const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得厂家日发电量寄存器和小数位配置。 */

        if((inv == RT_NULL) || (protocol == RT_NULL) ||
           (protocol->daily_energy.reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED)) /* 空档案、未匹配协议或未配置寄存器时返回固定长度FF。 */
        {
            rt_memset(field, 0xFF, point->data_len);
            continue;
        }
        dlt645_append_value(&inv->daily_energy, protocol->daily_energy.decimal_places,
                            2U, (uint8_t)point->data_len, RT_FALSE, field); /* 源值统一换算为XXXXXX.XX无符号BCD。 */
    }

    *data_len = required_len; /* 单台返回4字节，全量返回12×4字节。 */
    return RT_EOK;
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

/* 将040008xx最低字节定义的外部端口号映射到配置结构使用的内部UART编号。 */
static rt_err_t dlt645_serial_port_to_uart(uint8_t port_number, uint16_t *uart_no)
{
    switch(port_number) /* 端口顺序严格对应最新645点表，07无线串口不在支持范围内。 */
    {
        case 0x00U: *uart_no = UART3_NO; break; /* 04000800对应RS485-I。 */
        case 0x01U: *uart_no = UART6_NO; break; /* 04000801对应RS485-II。 */
        case 0x02U: *uart_no = UART7_NO; break; /* 04000802对应RJ45-1-I。 */
        case 0x03U: *uart_no = UART5_NO; break; /* 04000803对应RJ45-1-II。 */
        case 0x04U: *uart_no = UART4_NO; break; /* 04000804对应RJ45-2-I。 */
        case 0x05U: *uart_no = UART1_NO; break; /* 04000805对应RJ45-2-II。 */
        case 0x06U: *uart_no = UART8_NO; break; /* 04000806对应载波口。 */
        default: return -RT_EINVAL; /* 其他编号包括无线口均不允许通过本组数据标识访问。 */
    }
    return RT_EOK;
}

/* 读取04000800～04000806串口参数，返回4字节小端波特率和1字节校验格式。 */
rt_err_t dlt645_read_serial_parameter(const Dlt645PointTypeDef *point, uint32_t id,
                                      uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint16_t uart_no; /* DI0映射得到的内部UART配置数组下标。 */
    uint32_t baud; /* 当前串口波特率，仅允许点表约定的五种枚举值。 */
    uint16_t check_format; /* 当前校验格式，1/2/3依次表示8N1、8O1和8E1。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != DLT645_SERIAL_PARAMETER_LEN) || (capacity < point->data_len)) /* 一次完成处理边界所需的指针、长度和容量检查。 */
    {
        return -RT_EINVAL;
    }
    if(dlt645_serial_port_to_uart((uint8_t)id, &uart_no) != RT_EOK) /* DI0必须对应七个受支持的有线或载波端口之一。 */
    {
        return -RT_EINVAL;
    }

    baud = ctu_cfg.uart_baud[uart_no]; /* 配置值在启动时已由配置模块完成加载和校验。 */
    check_format = ctu_cfg.uart_check[uart_no]; /* 读取保存值，不读取尚未重启的硬件寄存器状态。 */
    if((baud_check(baud) != 0) || (check_format < 1U) || (check_format > 3U)) /* 非法配置不能伪装成正常645参数回复。 */
    {
        return -RT_EINVAL;
    }

    data[0] = (uint8_t)baud; /* 波特率按小端原始整数传输最低字节。 */
    data[1] = (uint8_t)(baud >> 8); /* 波特率第2字节。 */
    data[2] = (uint8_t)(baud >> 16); /* 波特率第3字节。 */
    data[3] = (uint8_t)(baud >> 24); /* 波特率最高字节。 */
    data[4] = (uint8_t)check_format; /* 校验格式占1字节，数值范围固定为1～3。 */
    *data_len = point->data_len; /* 串口参数固定返回5字节业务数据。 */
    return RT_EOK;
}

/* 写入04000800～04000806串口参数，校验全部字段后保存但不立即重配UART。 */
rt_err_t dlt645_write_serial_parameter(const Dlt645PointTypeDef *point, uint32_t id,
                                       const uint8_t *data, uint16_t data_len,
                                       uint8_t *response, uint16_t response_capacity,
                                       uint16_t *response_len)
{
    uint16_t uart_no; /* DI0映射得到的内部UART配置数组下标。 */
    uint32_t baud; /* 从4字节小端原始数据解出的目标波特率。 */
    uint8_t check_format; /* 写请求携带的目标校验格式枚举。 */

    RT_UNUSED(response); /* 标准写成功应答不携带额外业务数据。 */
    RT_UNUSED(response_capacity);
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) ||
       (point->data_len != DLT645_SERIAL_PARAMETER_LEN) || (data_len != point->data_len)) /* 写数据必须完整包含5字节串口参数。 */
    {
        return -RT_EINVAL;
    }
    if(dlt645_serial_port_to_uart((uint8_t)id, &uart_no) != RT_EOK) /* 禁止通过未定义或无线端口编号修改配置。 */
    {
        return -RT_EINVAL;
    }

    baud = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24); /* 按点表规定的小端原始整数恢复波特率。 */
    check_format = data[4]; /* 第5字节直接表示1～3校验格式枚举。 */
    if((baud_check(baud) != 0) || (check_format < 1U) || (check_format > 3U)) /* 任一字段非法时不允许部分更新配置。 */
    {
        return -RT_EINVAL;
    }

    ctu_cfg.uart_baud[uart_no] = baud; /* 仅更新配置值，当前UART硬件继续使用旧参数直到人工重启。 */
    ctu_cfg.uart_check[uart_no] = check_format; /* 校验格式与波特率在同一次校验通过后一起提交。 */
    ctu_cfg_save(); /* 串口参数写成功后立即持久化，保证人工重启后生效。 */
    *response_len = 0U; /* 成功时由顶层发送不带额外数据的标准写应答。 */
    return RT_EOK;
}

/* 读取04000900全局周期抄读间隔，按两字节低字节在前BCD返回秒数。 */
rt_err_t dlt645_read_poll_interval(const Dlt645PointTypeDef *point, uint32_t id,
                                   uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    uint16_t interval_seconds = ctu_cfg.poll_interval_seconds; /* 配置中的当前全局轮询秒数。 */

    RT_UNUSED(id); /* 04000900为固定数据标识，不使用DI0选择端口。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != DLT645_POLL_INTERVAL_LEN) || (capacity < point->data_len)) /* 读取固定需要2字节输出空间。 */
    {
        return -RT_EINVAL;
    }
    if((interval_seconds < DLT645_POLL_INTERVAL_MIN) || (interval_seconds > DLT645_POLL_INTERVAL_MAX) ||
       (dlt645_encode_bcd(interval_seconds, data, DLT645_POLL_INTERVAL_LEN, RT_FALSE) != RT_EOK)) /* 越界或无法编码的配置值不能正常回复。 */
    {
        return -RT_EINVAL;
    }

    *data_len = point->data_len; /* 周期抄读间隔固定返回2字节BCD。 */
    return RT_EOK;
}

/* 写入04000900全局周期抄读间隔，合法值立即用于后续调度并保存到Flash。 */
rt_err_t dlt645_write_poll_interval(const Dlt645PointTypeDef *point, uint32_t id,
                                    const uint8_t *data, uint16_t data_len,
                                    uint8_t *response, uint16_t response_capacity,
                                    uint16_t *response_len)
{
    int32_t interval_seconds; /* 从两字节BCD解码得到的全局轮询秒数。 */

    RT_UNUSED(id); /* 04000900为固定数据标识。 */
    RT_UNUSED(response); /* 标准写成功应答不携带额外业务数据。 */
    RT_UNUSED(response_capacity);
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) ||
       (point->data_len != DLT645_POLL_INTERVAL_LEN) || (data_len != point->data_len)) /* 写请求必须携带完整2字节BCD秒数。 */
    {
        return -RT_EINVAL;
    }
    if((dlt645_bcd_decode_u32(data, data_len, &interval_seconds) != RT_EOK) ||
       (interval_seconds < DLT645_POLL_INTERVAL_MIN) || (interval_seconds > DLT645_POLL_INTERVAL_MAX)) /* 非法BCD或超出5～3600秒均返回645写错误。 */
    {
        return -RT_EINVAL;
    }

    ctu_cfg.poll_interval_seconds = (uint16_t)interval_seconds; /* 校验成功后更新全局轮询配置，调度器下一次判断即可使用。 */
    ctu_cfg_save(); /* 周期抄读间隔写成功后立即保存。 */
    *response_len = 0U; /* 成功时由顶层发送不带额外数据的标准写应答。 */
    return RT_EOK;
}

/* 处理04000A00～04000C00设备动作，三个命令都只接受一字节BCD数值1。 */
rt_err_t dlt645_write_device_action(const Dlt645PointTypeDef *point, uint32_t id,
                                    const uint8_t *data, uint16_t data_len,
                                    uint8_t *response, uint16_t response_capacity,
                                    uint16_t *response_len)
{
    int32_t action_value; /* 动作值必须解码为1，其他值统一返回645写错误。 */

    RT_UNUSED(response); /* 标准写成功应答不携带额外业务数据。 */
    RT_UNUSED(response_capacity);
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) ||
       (point->data_len != 1U) || (data_len != point->data_len)) /* 三个动作数据标识均要求恰好1字节写数据。 */
    {
        return -RT_EINVAL;
    }
    if((dlt645_bcd_decode_u32(data, data_len, &action_value) != RT_EOK) || (action_value != 1)) /* 写0或其他数值均不执行任何动作。 */
    {
        return -RT_EINVAL;
    }

    switch(id) /* 使用完整数据标识区分动作，防止相邻DI误触发危险操作。 */
    {
        case 0x04000A00U:
            reboot(); /* 自定义reboot只停止喂狗并返回，约10秒后由看门狗完成设备重启。 */
            break;
        case 0x04000B00U:
            Clear_Events(EVT_CLASS_MAX); /* 按用户确认的统一接口清除所有事件记录。 */
            break;
        case 0x04000C00U:
            set_default_data(); /* 恢复默认配置并保存，但不自动重启，等待人工重启后整体生效。 */
            break;
        default:
            return -RT_EINVAL; /* 仅允许点表明确列出的三个动作标识进入执行路径。 */
    }

    *response_len = 0U; /* 动作调用返回后，由顶层发送不带额外数据的成功应答。 */
    return RT_EOK;
}

/* 读取0400040F位置信息，按标准顺序编码经度、纬度和高度，数据尚未执行加0x33。 */
rt_err_t dlt645_read_location(const Dlt645PointTypeDef *point, uint32_t id,
                              uint8_t *data, uint16_t capacity, uint16_t *data_len)
{
    RT_UNUSED(id); /* 0400040F是固定数据标识，不使用DI0选择档案或数据块。 */
    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL) ||
       (point->data_len != DLT645_LOCATION_DATA_LEN) || (capacity < point->data_len)) /* 一次检查点描述、输出指针及11字节容量。 */
    {
        return -RT_EINVAL;
    }
    if((ctu_cfg.longitude > DLT645_LONGITUDE_MAX) ||
       (ctu_cfg.latitude > DLT645_LATITUDE_MAX) ||
       (ctu_cfg.altitude > DLT645_ALTITUDE_MAX)) /* 配置中的越界位置不能截断为看似有效的BCD数据。 */
    {
        return -RT_EINVAL;
    }
    if((dlt645_encode_bcd((int32_t)ctu_cfg.longitude, &data[0], 4U, RT_FALSE) != RT_EOK) ||
       (dlt645_encode_bcd((int32_t)ctu_cfg.latitude, &data[4], 4U, RT_FALSE) != RT_EOK) ||
       (dlt645_encode_bcd((int32_t)ctu_cfg.altitude, &data[8], 3U, RT_FALSE) != RT_EOK)) /* 三个字段必须全部成功编码才允许回复。 */
    {
        return -RT_EINVAL;
    }

    *data_len = point->data_len; /* 标准位置信息固定返回11字节业务数据。 */
    return RT_EOK;
}

/* 写入0400040F位置信息，全部字段校验成功后一次性更新配置并调用ctu_cfg_save持久化。 */
rt_err_t dlt645_write_location(const Dlt645PointTypeDef *point, uint32_t id,
                               const uint8_t *data, uint16_t data_len,
                               uint8_t *response, uint16_t response_capacity,
                               uint16_t *response_len)
{
    int32_t longitude; /* 解码后的经度定点整数，单位0.0001度。 */
    int32_t latitude;  /* 解码后的纬度定点整数，单位0.0001度。 */
    int32_t altitude;  /* 解码后的高度定点整数，单位0.01米。 */

    RT_UNUSED(id); /* 固定数据标识不需要再次判断DI0。 */
    RT_UNUSED(response); /* 标准写数据成功应答不携带业务数据。 */
    RT_UNUSED(response_capacity);
    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL) ||
       (point->data_len != DLT645_LOCATION_DATA_LEN) || (data_len != point->data_len)) /* 写入业务区必须完整包含11字节位置信息。 */
    {
        return -RT_EINVAL;
    }
    if((dlt645_bcd_decode_u32(&data[0], 4U, &longitude) != RT_EOK) ||
       (dlt645_bcd_decode_u32(&data[4], 4U, &latitude) != RT_EOK) ||
       (dlt645_bcd_decode_u32(&data[8], 3U, &altitude) != RT_EOK)) /* 任一字段包含非法BCD时整组写入失败。 */
    {
        return -RT_EINVAL;
    }
    if(((uint32_t)longitude > DLT645_LONGITUDE_MAX) ||
       ((uint32_t)latitude > DLT645_LATITUDE_MAX) ||
       ((uint32_t)altitude > DLT645_ALTITUDE_MAX)) /* 经度、纬度和高度必须同时处于允许范围。 */
    {
        return -RT_EINVAL;
    }

    ctu_cfg.longitude = (uint32_t)longitude; /* 完整校验后再提交新经度，避免失败时只更新部分字段。 */
    ctu_cfg.latitude = (uint32_t)latitude; /* 纬度与经度在同一次写请求中原子更新内存值。 */
    ctu_cfg.altitude = (uint32_t)altitude; /* 高度属于同一标准数据标识，随经纬度一起更新。 */
    ctu_cfg_save(); /* 位置信息写入成功后保存到配置A/B区，重新上电后继续有效。 */
    *response_len = 0U; /* 成功时由顶层发送数据域长度为0的0x94标准写应答。 */
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

/* 根据04E60B或04E60C取得时段控制方式、单个数值长度和645目标小数位。 */
static rt_err_t dlt645_time_control_format(uint32_t id,
                                           Time_Ctrl_Mode_t *mode,
                                           uint8_t *value_len,
                                           uint8_t *target_decimals)
{
    uint8_t point_type = (uint8_t)(id >> 8); /* DI1区分有功数值时段控和有功百分比时段控。 */

    if(point_type == 0x0BU)
    {
        *mode = TIME_CTRL_ACTIVE_POWER_VALUE;
        *value_len = 4U;       /* 有功功率格式XXXX.XXXX固定占4字节。 */
        *target_decimals = 4U; /* 有功功率645字段固定保留4位小数。 */
        return RT_EOK;
    }
    if(point_type == 0x0CU)
    {
        *mode = TIME_CTRL_ACTIVE_POWER_PERCENT;
        *value_len = 2U;       /* 有功百分比格式XXX.X固定占2字节。 */
        *target_decimals = 1U; /* 有功百分比645字段固定保留1位小数。 */
        return RT_EOK;
    }
    return -RT_EINVAL; /* 其他数据标识不属于本时段控制接口。 */
}

/* 取得指定档案和控制方式对应的厂家控制寄存器配置。 */
static const Inv_CtrlRegBlk_t *dlt645_time_control_reg(uint8_t archive_index, Time_Ctrl_Mode_t mode)
{
    const Inv_Proto_t *protocol = Inv_Archive_Get_Protocol(archive_index); /* 取得档案绑定的厂家协议。 */

    if(protocol == RT_NULL) /* 空档案或协议未匹配时没有可用的小数位和控制能力配置。 */
    {
        return RT_NULL;
    }
    return (mode == TIME_CTRL_ACTIVE_POWER_VALUE) ?
           &protocol->ctrl.active_pwr_ctrl : &protocol->ctrl.active_pwr_pct_ctrl; /* 两类时段控复用已有数值控制寄存器。 */
}

/* 将低字节在前的hhmm压缩BCD解码为小时和分钟。 */
static rt_err_t dlt645_decode_hhmm(const uint8_t *data, uint8_t *hour, uint8_t *minute)
{
    int32_t decoded_hour;   /* BCD小时转换后的整数值。 */
    int32_t decoded_minute; /* BCD分钟转换后的整数值。 */

    if((dlt645_bcd_decode_u32(&data[0], 1U, &decoded_minute) != RT_EOK) ||
       (dlt645_bcd_decode_u32(&data[1], 1U, &decoded_hour) != RT_EOK) ||
       (decoded_hour > 23) || (decoded_minute > 59)) /* 线上顺序为分、时，且两个字段必须符合时钟范围。 */
    {
        return -RT_EINVAL;
    }
    *hour = (uint8_t)decoded_hour;     /* 范围确认后输出小时。 */
    *minute = (uint8_t)decoded_minute; /* 范围确认后输出分钟。 */
    return RT_EOK;
}

/* 将小时和分钟编码成低字节在前的hhmm压缩BCD。 */
static rt_err_t dlt645_encode_hhmm(uint8_t hour, uint8_t minute, uint8_t *data)
{
    if((hour > 23U) || (minute > 59U)) /* 内部时间异常时禁止输出非法BCD时间。 */
    {
        return -RT_EINVAL;
    }
    if((dlt645_encode_bcd(minute, &data[0], 1U, RT_FALSE) != RT_EOK) ||
       (dlt645_encode_bcd(hour, &data[1], 1U, RT_FALSE) != RT_EOK)) /* 645线上低地址字节先发送分钟，再发送小时。 */
    {
        return -RT_EINVAL;
    }
    return RT_EOK;
}

/* 把单台645时段数据块解析为绑定接收当天的一次性时段命令。 */
static rt_err_t dlt645_decode_time_control_block(uint32_t id,
                                                 uint8_t archive_index,
                                                 const uint8_t *block,
                                                 Time_Ctrl_Command_t *command)
{
    struct tm current_time = get_local_time_t(); /* 04E60B和04E60C都使用接收当天的本地日期。 */
    Time_Ctrl_Mode_t mode;                       /* 当前数据标识对应的时段控制方式。 */
    const Inv_CtrlRegBlk_t *control_reg;         /* 目标厂家协议对应的数值控制配置。 */
    uint8_t value_len;                           /* 单个控制值在645数据块中的长度。 */
    uint8_t source_decimals;                     /* 645控制值字段的小数位。 */
    uint8_t period_index;                        /* 当前解析的第1或第2时段下标。 */

    if(dlt645_time_control_format(id, &mode, &value_len, &source_decimals) != RT_EOK) /* 数据标识必须属于两类时段控制之一。 */
    {
        return -RT_EINVAL;
    }
    rt_memset(command, 0, sizeof(*command)); /* 禁用时段及秒字段默认保持为0。 */
    command->archive_index = archive_index; /* 命令档案下标与DI0或全量槽位一一对应。 */

    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index)
    {
        uint16_t time_offset = period_index * 4U; /* 当前时段开始时间在单台块中的偏移。 */
        uint16_t value_offset = 8U + period_index * value_len; /* 当前时段控制值在单台块中的偏移。 */
        rt_bool_t start_ff = dlt645_field_is_ff(&block[time_offset], 2U); /* 开始时间是否全FF。 */
        rt_bool_t end_ff = dlt645_field_is_ff(&block[time_offset + 2U], 2U); /* 结束时间是否全FF。 */
        rt_bool_t value_ff = dlt645_field_is_ff(&block[value_offset], value_len); /* 控制值是否全FF。 */
        int32_t decoded_value; /* 完成符号位BCD解码后的645定点控制值。 */

        if((start_ff == RT_TRUE) || (end_ff == RT_TRUE)) /* 开始或结束时间任一为全FF时，该时段不执行调控。 */
        {
            command->periods[period_index].enabled = RT_FALSE;
            continue; /* 禁用时段不再解析或校验其控制值，允许主站保留原数值。 */
        }
        if(value_ff == RT_TRUE) /* 开始和结束均有效但控制值为FF时无法执行明确调控。 */
        {
            return -RT_EINVAL;
        }
        if((current_time.tm_year < 70) ||
           (dlt645_decode_hhmm(&block[time_offset], &command->periods[period_index].start.hour,
                               &command->periods[period_index].start.minute) != RT_EOK) ||
           (dlt645_decode_hhmm(&block[time_offset + 2U], &command->periods[period_index].end.hour,
                               &command->periods[period_index].end.minute) != RT_EOK)) /* RTC日期和两个hhmm时间必须有效。 */
        {
            return -RT_EINVAL;
        }
        command->periods[period_index].start.year = (uint16_t)(current_time.tm_year + 1900); /* 开始日期绑定接收当天。 */
        command->periods[period_index].start.month = (uint8_t)(current_time.tm_mon + 1);
        command->periods[period_index].start.day = (uint8_t)current_time.tm_mday;
        command->periods[period_index].end.year = command->periods[period_index].start.year; /* 结束日期与开始日期相同，禁止跨天。 */
        command->periods[period_index].end.month = command->periods[period_index].start.month;
        command->periods[period_index].end.day = command->periods[period_index].start.day;
        command->periods[period_index].mode = mode; /* 同一个645数据标识内两个时段使用相同调节方式。 */
        command->periods[period_index].enabled = RT_TRUE;

        control_reg = dlt645_time_control_reg(archive_index, mode); /* 取得厂家目标小数位用于写值换算。 */
        if((control_reg == RT_NULL) ||
           (dlt645_bcd_decode_s32(&block[value_offset], value_len, &decoded_value) != RT_EOK) ||
           ((mode == TIME_CTRL_ACTIVE_POWER_PERCENT) &&
            ((decoded_value < -1000) || (decoded_value > 1000))) ||
           (dlt645_rescale_value(decoded_value, source_decimals, control_reg->decimal_places,
                                 &command->periods[period_index].value) != RT_EOK)) /* 配置、BCD、百分比范围和定点换算必须全部有效。 */
        {
            return -RT_EINVAL;
        }
    }
    return RT_EOK;
}

/* 将当前保存的一台时段命令编码成04E60B或04E60C单台数据块。 */
static rt_err_t dlt645_encode_time_control_block(uint32_t id,
                                                 uint8_t archive_index,
                                                 const Time_Ctrl_Command_t *command,
                                                 uint8_t *block)
{
    Time_Ctrl_Mode_t requested_mode;             /* 当前读取数据标识要求的时段控制方式。 */
    const Inv_CtrlRegBlk_t *control_reg;          /* 厂家控制值缓存所使用的小数位配置。 */
    uint8_t value_len;                            /* 单个控制值的645字段长度。 */
    uint8_t target_decimals;                      /* 645控制值字段的小数位。 */
    uint8_t period_index;                         /* 当前编码的第1或第2时段下标。 */
    uint16_t block_len;                           /* 当前数据标识对应的单台数据块长度。 */

    if(dlt645_time_control_format(id, &requested_mode, &value_len, &target_decimals) != RT_EOK) /* 先确定目标格式。 */
    {
        return -RT_EINVAL;
    }
    block_len = 8U + TIME_CTRL_PERIOD_COUNT * value_len; /* 四个hhmm共8字节，尾部再放两个控制值。 */
    rt_memset(block, 0xFF, block_len); /* 禁用或属于另一控制方式的时段默认保持全FF。 */
    control_reg = dlt645_time_control_reg(archive_index, requested_mode); /* 读取值需要从厂家小数位换回645精度。 */
    if(control_reg == RT_NULL) /* 当前厂家不支持该控制方式时整个单台块返回FF。 */
    {
        return RT_EOK;
    }

    for(period_index = 0U; period_index < TIME_CTRL_PERIOD_COUNT; ++period_index)
    {
        uint16_t time_offset = period_index * 4U; /* 当前时段开始时间字段偏移。 */
        uint16_t value_offset = 8U + period_index * value_len; /* 当前时段控制值字段偏移。 */
        int32_t scaled_value; /* 从厂家控制精度换算到645目标精度后的定点值。 */
        uint8_t encoded_start[2]; /* 完整编码成功前暂存开始hhmm，避免留下半组有效字段。 */
        uint8_t encoded_end[2];   /* 完整编码成功前暂存结束hhmm。 */
        uint8_t encoded_value[4]; /* 两类控制值最大占4字节。 */

        if((command->periods[period_index].enabled != RT_TRUE) ||
           (command->periods[period_index].mode != requested_mode)) /* 禁用或属于另一数据标识的时段继续使用FF。 */
        {
            continue;
        }
        if((dlt645_encode_hhmm(command->periods[period_index].start.hour,
                               command->periods[period_index].start.minute, encoded_start) != RT_EOK) ||
           (dlt645_encode_hhmm(command->periods[period_index].end.hour,
                               command->periods[period_index].end.minute, encoded_end) != RT_EOK) ||
           (dlt645_rescale_value(command->periods[period_index].value, control_reg->decimal_places,
                                 target_decimals, &scaled_value) != RT_EOK) ||
           (dlt645_encode_bcd(scaled_value, encoded_value, value_len, RT_TRUE) != RT_EOK)) /* 时间、精度和有符号BCD必须整体编码成功。 */
        {
            continue;
        }
        rt_memcpy(&block[time_offset], encoded_start, sizeof(encoded_start)); /* 全部字段有效后写入当前时段开始时间。 */
        rt_memcpy(&block[time_offset + 2U], encoded_end, sizeof(encoded_end)); /* 写入当前时段结束时间。 */
        rt_memcpy(&block[value_offset], encoded_value, value_len); /* 写入当前时段控制值。 */
    }
    return RT_EOK;
}

/* 读取单台或全部逆变器的有功数值或百分比时段控制配置。 */
rt_err_t dlt645_read_time_control(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data,
                                  uint16_t capacity, uint16_t *data_len)
{
    uint8_t selector = (uint8_t)id; /* DI0选择单台逆变器或全部12个档案槽位。 */
    uint8_t first_archive;          /* 本次读取的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次需要返回的固定单台数据块数量。 */
    uint8_t archive_offset;         /* 当前处理的相对档案槽位。 */
    uint16_t required_len;          /* 单台或全量读取的完整业务数据长度。 */

    if((point == RT_NULL) || (data == RT_NULL) || (data_len == RT_NULL)) /* 公共入口的重要指针只检查一次。 */
    {
        return -RT_EINVAL;
    }
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* 选择器已由分发层校验。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* FF仍为每台独立时间和数值。 */
    required_len = point->data_len * archive_count; /* 04E60B全量192字节，04E60C全量144字节。 */
    if(capacity < required_len) /* 禁止发送被截断的时段控制数据块。 */
    {
        return -RT_EINVAL;
    }
    rt_memset(data, 0xFF, required_len); /* 空档案、未配置和不支持默认返回对应单台块全FF。 */

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前读取的实际档案下标。 */
        Time_Ctrl_Command_t command; /* 从时段线程邮箱取得的完整命令快照。 */
        rt_bool_t enabled;           /* 当前档案是否启用了时段计划。 */

        if((Time_Ctrl_Get(archive_index, &command, &enabled) != TIME_CTRL_RESULT_OK) ||
           (enabled != RT_TRUE)) /* 空档案、未初始化或已停止计划保持全FF。 */
        {
            continue;
        }
        (void)dlt645_encode_time_control_block(id, archive_index, &command,
                                               &data[archive_offset * point->data_len]); /* 单个时段编码失败时对应字段保持FF。 */
    }
    *data_len = required_len; /* 单台和全量都返回固定长度，便于主站按槽位解析。 */
    return RT_EOK;
}

/* 写入单台或全部逆变器的当天一次性时段计划，全部预检查通过后再统一更新邮箱。 */
rt_err_t dlt645_write_time_control(const Dlt645PointTypeDef *point, uint32_t id, const uint8_t *data,
                                   uint16_t data_len, uint8_t *response,
                                   uint16_t response_capacity, uint16_t *response_len)
{
    uint8_t selector = (uint8_t)id; /* DI0选择单台或全部12个独立时段数据块。 */
    uint8_t first_archive;          /* 本次写入的首个档案槽位下标。 */
    uint8_t archive_count;          /* 本次写入包含的固定单台块数量。 */
    uint8_t archive_offset;         /* 当前预检查或提交的相对档案槽位。 */
    Time_Ctrl_Command_t commands[INVERTER_ARCHIVE_MAX_COUNT]; /* 预解析完成且绑定接收当天的逐台命令。 */
    rt_bool_t apply[INVERTER_ARCHIVE_MAX_COUNT]; /* RT_TRUE表示当前档案需要更新或停止时段计划。 */

    if((point == RT_NULL) || (data == RT_NULL) || (response_len == RT_NULL)) /* 公共写入口的重要指针只检查一次。 */
    {
        return -RT_EINVAL;
    }
    RT_UNUSED(response); /* 时段计划写入使用标准无数据写应答。 */
    RT_UNUSED(response_capacity);
    *response_len = 0U; /* 明确通知分发层不要生成逐台状态数据域。 */
    first_archive = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? 0U : (uint8_t)(selector - 1U); /* 选择器已经由分发层验证。 */
    archive_count = (selector == DLT645_VARIABLE_ALL_SELECTOR) ? INVERTER_ARCHIVE_MAX_COUNT : 1U; /* 全量仍按12个完整单台块解析。 */
    if(data_len != (uint16_t)(point->data_len * archive_count)) /* 单台和全量长度必须与点表完全一致。 */
    {
        return -RT_EINVAL;
    }
    rt_memset(apply, 0, sizeof(apply)); /* 空档案配全FF允许跳过，不修改任何邮箱。 */

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        uint8_t archive_index = first_archive + archive_offset; /* 当前数据块对应的实际档案槽位。 */
        const uint8_t *block = &data[archive_offset * point->data_len]; /* 当前逆变器的完整时段控制数据块。 */
        rt_bool_t period1_disabled = dlt645_field_is_ff(&block[0], 2U) ||
                                     dlt645_field_is_ff(&block[2], 2U); /* 第1时段开始或结束为FF即不执行。 */
        rt_bool_t period2_disabled = dlt645_field_is_ff(&block[4], 2U) ||
                                     dlt645_field_is_ff(&block[6], 2U); /* 第2时段采用相同的禁用规则。 */

        if(Inv_Data_Get(archive_index) == RT_NULL) /* 空档案只允许两个时段都处于不调控状态。 */
        {
            if((period1_disabled != RT_TRUE) || (period2_disabled != RT_TRUE)) /* 任一时段时间完整都表示存在对空档案的控制意图。 */
            {
                return -RT_EINVAL;
            }
            continue;
        }
        if(dlt645_decode_time_control_block(id, archive_index, block, &commands[archive_offset]) != RT_EOK) /* 单台块的时间、FF组合、数值或精度必须合法。 */
        {
            return -RT_EINVAL;
        }
        if(Time_Ctrl_Check(&commands[archive_offset]) != TIME_CTRL_RESULT_OK) /* 检查工作窗口、重叠、控制能力和恢复能力。 */
        {
            return -RT_EINVAL;
        }
        apply[archive_offset] = RT_TRUE; /* 全部预检查完成后才会实际修改这个档案的邮箱。 */
    }

    for(archive_offset = 0U; archive_offset < archive_count; ++archive_offset)
    {
        if((apply[archive_offset] == RT_TRUE) &&
           (Time_Ctrl_Set(commands[archive_offset]) != TIME_CTRL_RESULT_OK)) /* 两个时段全FF由Set统一转换为停止计划。 */
        {
            return -RT_ERROR; /* 极短窗口内档案或线程状态变化时返回写异常。 */
        }
    }
    return RT_EOK; /* 所有需要处理的档案邮箱均已成功更新或停止。 */
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
