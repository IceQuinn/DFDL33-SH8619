#ifndef __DLT645_DATA_API_H__
#define __DLT645_DATA_API_H__

#include <stdint.h>
#include <rtthread.h>

/* 点表权限使用位标志，便于在统一分发入口直接检查读写能力。 */
typedef enum
{
    DLT645_ACCESS_READ  = 1U << 0, /* 该数据标识允许主站使用读数据命令抄读。 */
    DLT645_ACCESS_WRITE = 1U << 1, /* 该数据标识允许主站使用写数据命令设置。 */
} Dlt645AccessTypeDef;

/* 描述点表数据在执行统一加减0x33之外所采用的业务编码格式。 */
typedef enum
{
    DLT645_CODEC_BCD = 0,       /* 无符号压缩BCD，低地址字节表示低位十进制数。 */
    DLT645_CODEC_SBCD,          /* 最高有效字节的最高位携带正负符号的压缩BCD。 */
    DLT645_CODEC_ASCII,         /* 按规范规定长度传输的ASCII字符数据。 */
    DLT645_CODEC_RAW,           /* 不做数值转换、按原始字节组织的数据。 */
    DLT645_CODEC_CUSTOM,        /* 日期、档案或混合数据块等由点处理函数自行编解码。 */
} Dlt645CodecTypeDef;

/* 描述数据标识最低字节DI0的选择器含义，多个能力可通过按位或组合。 */
typedef enum
{
    DLT645_SELECTOR_NONE   = 0,       /* DI0是固定数据标识的一部分，不表示设备编号。 */
    DLT645_SELECTOR_DEVICE = 1U << 0, /* DI0允许取01～0C，分别选择逆变器1～12。 */
    DLT645_SELECTOR_ALL    = 1U << 1, /* DI0允许取FF，表示读取或设置全部逆变器。 */
} Dlt645SelectorTypeDef;

struct Dlt645PointTypeDef; /* 提前声明点描述结构，供下方回调函数类型引用。 */

/* 读取回调把业务数据写入未加0x33的data，并通过data_len返回实际数据字节数。 */
typedef rt_err_t (*Dlt645ReadHandler)(const struct Dlt645PointTypeDef *point,
                                     uint32_t id,
                                     uint8_t *data,
                                     uint16_t capacity,
                                     uint16_t *data_len);
/* 写入回调接收解析层已经减0x33并移除安全字段后的实际写数据。 */
typedef rt_err_t (*Dlt645WriteHandler)(const struct Dlt645PointTypeDef *point,
                                      uint32_t id,
                                      const uint8_t *data,
                                      uint16_t data_len);

/* 描述一个645数据标识的匹配方式、权限、编码、范围以及对应业务数据接口。 */
typedef struct Dlt645PointTypeDef
{
    uint32_t id;               /* 点表基准数据标识，动态设备点通常把DI0配置为00。 */
    uint32_t mask;             /* 数据标识匹配掩码，掩码为0的位不参与查表比较。 */
    uint8_t access;            /* Dlt645AccessTypeDef按位组合得到的读写权限。 */
    uint8_t codec;             /* Dlt645CodecTypeDef定义的数据编码方式。 */
    uint8_t selector;          /* Dlt645SelectorTypeDef按位组合得到的DI0取值范围。 */
    uint16_t data_len;         /* 写入数据固定长度，读取时作为该点预期数据长度。 */
    int32_t scale;             /* 定点数倍率，例如XXX.X对应10，当前运行状态对应1。 */
    int32_t write_min;         /* 写入值完成解码并除以倍率后的允许下限。 */
    int32_t write_max;         /* 写入值完成解码并除以倍率后的允许上限。 */
    Dlt645ReadHandler read;    /* 读取处理函数，只生成未加0x33的纯数据域。 */
    Dlt645WriteHandler write;  /* 写入处理函数，负责校验业务值并提交实际控制。 */
    const char *name;          /* 用于运行日志和问题定位的点表英文名称。 */
} Dlt645PointTypeDef;

/* 读取指定逆变器或全部逆变器的三相电压数据块。 */
rt_err_t dlt645_read_voltage(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 读取指定逆变器或全部逆变器的三相电流数据块。 */
rt_err_t dlt645_read_current(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 读取指定逆变器或全部逆变器的总、A、B、C相有功功率数据块。 */
rt_err_t dlt645_read_active_power(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 读取指定逆变器或全部逆变器的总、A、B、C相无功功率数据块。 */
rt_err_t dlt645_read_reactive_power(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 读取指定逆变器或全部逆变器的总、A、B、C相功率因数数据块。 */
rt_err_t dlt645_read_power_factor(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 按电压、电流、有功、无功和功率因数顺序读取指定逆变器的全部变量。 */
rt_err_t dlt645_read_all_variables(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);

/* 读取指定逆变器或全部逆变器的额定有功功率Pn。 */
rt_err_t dlt645_read_Pn(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);
/* 读取指定逆变器或全部逆变器的额定无功功率Qn。 */
rt_err_t dlt645_read_Qn(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);

/* 读取指定逆变器或全部逆变器的输出类型，00表示单相、01表示三相。 */
rt_err_t dlt645_read_output_type(const Dlt645PointTypeDef *point, uint32_t id, uint8_t *data, uint16_t capacity, uint16_t *data_len);

/* 从实时数据中心读取指定逆变器运行状态并编码为规范规定的单字节BCD。 */
rt_err_t dlt645_read_run_state(const Dlt645PointTypeDef *point,
                               uint32_t id,
                               uint8_t *data,
                               uint16_t capacity,
                               uint16_t *data_len);

/* 校验运行状态写入值并通过逆变器控制队列同步取得最终执行结果。 */
rt_err_t dlt645_write_run_state(const Dlt645PointTypeDef *point,
                                uint32_t id,
                                const uint8_t *data,
                                uint16_t data_len);

#endif /* __DLT645_DATA_API_H__ */
