#include "inv_protocol_print.h"

#include <stdlib.h>
#include <rtthread.h>

#include "inverter_protocol_library.h"

/* 返回协议数据类型对应的可读名称。 */
static const char *inv_proto_data_type_name(uint8_t data_type)
{
    /* 每种协议数据类型返回固定文本，未知值统一返回UNKNOWN。 */
    switch(data_type) {
    case TYPE_I8:
        return "INT8";
    case TYPE_U8:
        return "UINT8";
    case TYPE_I16:
        return "INT16";
    case TYPE_U16:
        return "UINT16";
    case TYPE_I32:
        return "INT32";
    case TYPE_U32:
        return "UINT32";
    case TYPE_FLOAT32:
        return "FLOAT32";
    case TYPE_FLOAT64:
        return "FLOAT64";
    case TYPE_ASCII:
        return "ASCII";
    case TYPE_BCD:
        return "BCD";
    case TYPE_BCD_TIME:
        return "BCD_TIME";
    case TYPE_BIT_FIELD:
        return "BIT_FIELD";
    default:
        return "UNKNOWN";
    }
}

/* 根据数据类型和字节序返回可读的字节排列名称。 */
static const char *inv_proto_byte_order_name(uint8_t data_type, uint8_t byte_order)
{
    /* 每种字节序分别判断16位、32位或通用名称。 */
    switch(byte_order) {
    case Type_Byte_ABCD:
        /* 16位整数的正常字节序为AB。 */
        if((data_type == TYPE_I16) || (data_type == TYPE_U16)) {
            return "AB";
        }

        /* 32位整数和单精度浮点数的正常字节序为ABCD。 */
        if((data_type == TYPE_I32) || (data_type == TYPE_U32) || (data_type == TYPE_FLOAT32)) {
            return "ABCD";
        }

        return "ABCD";

    case Type_Byte_CDAB:
        /* 16位整数交换字节后为BA。 */
        if((data_type == TYPE_I16) || (data_type == TYPE_U16)) {
            return "BA";
        }

        /* 32位整数和单精度浮点数交换寄存器后为CDAB。 */
        if((data_type == TYPE_I32) || (data_type == TYPE_U32) || (data_type == TYPE_FLOAT32)) {
            return "CDAB";
        }

        return "CDAB";

    case Type_Byte_BADC:
        return "BADC";

    case Type_Byte_DCBA:
        return "DCBA";

    default:
        return "UNKNOWN";
    }
}

/* 打印数据类或参数类的只读寄存器表头。 */
static void inv_proto_print_read_table_header(const char *section_name)
{
    /* 输出只读寄存器各字段名称，后续数据类和参数类共用该表头。 */
    rt_kprintf("[%08d] %-20s %-10s %-7s %-8s %-14s %-12s %-7s\n", rt_tick_get(), section_name, "address", "count", "read_fc", "type", "order", "decimal");
}

/* 打印数据类或参数类中的一个只读寄存器配置。 */
static void inv_proto_print_reg(const char *name, const Inv_RegBlk_t *reg)
{
    /* 将寄存器数值字段和类型、字节序文本组合成一行输出。 */
    rt_kprintf("[%08d] %-20s %-10d %-7d %-8d %-14s %-12s %-7d\n", rt_tick_get(), name, reg->reg_addr, reg->reg_cnt, reg->read_func_code, inv_proto_data_type_name(reg->data_type), inv_proto_byte_order_name(reg->data_type, reg->byte_order), reg->decimal_places);
}

/* 打印控制类寄存器表头。 */
static void inv_proto_print_ctrl_table_header(const char *section_name)
{
    /* 控制表头使用write_fc区分只读寄存器表的read_fc。 */
    rt_kprintf("[%08d] %-20s %-10s %-7s %-8s %-14s %-12s %-7s\n", rt_tick_get(), section_name, "address", "count", "write_fc", "type", "order", "decimal");
}

/* 打印不带默认值的控制寄存器配置。 */
static void inv_proto_print_ctrl_reg(const char *name, const Inv_CtrlRegBlk_t *reg)
{
    /* 打印数值类控制寄存器，不包含开关机固定默认值。 */
    rt_kprintf("[%08d] %-20s %-10d %-7d %-8d %-14s %-12s %-7d\n", rt_tick_get(), name, reg->reg_addr, reg->reg_cnt, reg->write_func_code, inv_proto_data_type_name(reg->data_type), inv_proto_byte_order_name(reg->data_type, reg->byte_order), reg->decimal_places);
}

/* 打印开机或关机固定控制寄存器及其默认写入值。 */
static void inv_proto_print_default_ctrl_reg(const char *name, const Inv_CtrlDefaultRegBlk_t *reg)
{
    /* 在普通控制寄存器字段后额外打印协议规定的默认写入值。 */
    rt_kprintf("[%08d] %-20s %-10d %-7d %-8d %-14s %-12s %-7d default=%d\n", rt_tick_get(), name, reg->reg_addr, reg->reg_cnt, reg->write_func_code, inv_proto_data_type_name(reg->data_type), inv_proto_byte_order_name(reg->data_type, reg->byte_order), reg->decimal_places, reg->write_default_val);
}

/* 打印协议识别使用的特征寄存器配置。 */
static void inv_proto_print_feature(const Inv_Feature_t *feature)
{
    /* 特征寄存器单独成表，并输出自动识别使用的闭区间上下限。 */
    rt_kprintf("[%08d] %-20s %-10s %-7s %-8s %-14s %-12s %-7s %-12s %-12s\n", rt_tick_get(), "[feature]", "address", "count", "read_fc", "type", "order", "decimal", "lower", "upper");
    rt_kprintf("[%08d] %-20s %-10d %-7d %-8d %-14s %-12s %-7d %-12u %-12u\n", rt_tick_get(), "feature", feature->reg_addr, feature->reg_cnt, feature->read_func_code, inv_proto_data_type_name(feature->data_type), inv_proto_byte_order_name(feature->data_type, feature->byte_order), feature->decimal_places, (unsigned int)feature->lower_limit, (unsigned int)feature->upper_limit);
}

/* 按1～100协议序号获取协议对象，序号越界时返回RT_NULL。 */
const Inv_Proto_t *Inv_Proto_Get(uint16_t proto_number)
{
    /* 协议序号从1开始，0或超过协议库容量都属于越界。 */
    if((proto_number == 0) || (proto_number > INVERTER_PROTOCOL_LIBRARY_COUNT)) {
        return RT_NULL;
    }

    return &g_inv_proto_lib.proto[proto_number - 1];
}

/* 完整打印一条协议中的厂家信息、数据类、参数类和控制类。 */
static void inv_proto_print_one(uint16_t proto_number, uint16_t valid_number, const Inv_Proto_t *proto)
{
    const char *manufacturer_text;                           /* 最终打印的厂家名称文本。 */
    char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1]; /* 补结束符后的厂家名称。 */

    Inv_Archive_Copy_Mfr_Name(manufacturer, proto->mfr_info.name); /* 安全转换协议中的定长厂家名称。 */

    /* 厂家名称为空时使用固定文本，避免日志字段为空。 */
    if(manufacturer[0] == '\0') {
        manufacturer_text = "(empty)";
    }
    /* 非空厂家名称直接使用转换后的安全字符串。 */
    else {
        manufacturer_text = manufacturer;
    }

    rt_kprintf("[%08d] valid[%d] protocol[%d] manufacturer[%s] version[0x%04X]\n", rt_tick_get(), valid_number, proto_number, manufacturer_text, (unsigned int)proto->mfr_info.proto_ver);
    inv_proto_print_feature(&proto->feature);

    inv_proto_print_read_table_header("[data]");
    inv_proto_print_reg("Ua", &proto->data.Ux[ENUM_PHASE_A]);
    inv_proto_print_reg("Ub", &proto->data.Ux[ENUM_PHASE_B]);
    inv_proto_print_reg("Uc", &proto->data.Ux[ENUM_PHASE_C]);
    inv_proto_print_reg("Ia", &proto->data.Ix[ENUM_PHASE_A]);
    inv_proto_print_reg("Ib", &proto->data.Ix[ENUM_PHASE_B]);
    inv_proto_print_reg("Ic", &proto->data.Ix[ENUM_PHASE_C]);
    inv_proto_print_reg("Pa", &proto->data.Px[ENUM_PA]);
    inv_proto_print_reg("Pb", &proto->data.Px[ENUM_PB]);
    inv_proto_print_reg("Pc", &proto->data.Px[ENUM_PC]);
    inv_proto_print_reg("Pt", &proto->data.Px[ENUM_PT]);
    inv_proto_print_reg("Qa", &proto->data.Qx[ENUM_QA]);
    inv_proto_print_reg("Qb", &proto->data.Qx[ENUM_QB]);
    inv_proto_print_reg("Qc", &proto->data.Qx[ENUM_QC]);
    inv_proto_print_reg("Qt", &proto->data.Qx[ENUM_QT]);
    inv_proto_print_reg("PFa", &proto->data.PFx[ENUM_PFA]);
    inv_proto_print_reg("PFb", &proto->data.PFx[ENUM_PFB]);
    inv_proto_print_reg("PFc", &proto->data.PFx[ENUM_PFC]);
    inv_proto_print_reg("PFt", &proto->data.PFx[ENUM_PFT]);

    inv_proto_print_read_table_header("[parameter]");
    inv_proto_print_reg("device_no", &proto->param.dev_no);
    inv_proto_print_reg("pv_rated_p", &proto->param.pv_rated_active_pwr);
    inv_proto_print_reg("pv_rated_q", &proto->param.pv_rated_reactive_pwr);
    inv_proto_print_reg("set_voltage", &proto->param.set_volt);
    inv_proto_print_reg("output_type", &proto->param.output_type);

    inv_proto_print_ctrl_table_header("[control]");
    inv_proto_print_default_ctrl_reg("power_on", &proto->ctrl.pwr_on);
    inv_proto_print_default_ctrl_reg("power_off", &proto->ctrl.pwr_off);
    inv_proto_print_ctrl_reg("active_pwr_value", &proto->ctrl.active_pwr_ctrl);
    inv_proto_print_ctrl_reg("reactive_pwr_value", &proto->ctrl.reactive_pwr_ctrl);
    inv_proto_print_ctrl_reg("power_factor", &proto->ctrl.pwr_factor_ctrl);
    inv_proto_print_ctrl_reg("active_pwr_pct", &proto->ctrl.active_pwr_pct_ctrl);
    inv_proto_print_ctrl_reg("reactive_pwr_pct", &proto->ctrl.reactive_pwr_pct_ctrl);

    inv_proto_print_read_table_header("[daily]");
    inv_proto_print_reg("daily_generation", &proto->daily_generation);
}

/* 打印指定数量的协议槽位，count为0时默认打印前10条。 */
void Inv_Proto_Print(uint16_t count)
{
    uint16_t proto_number;     /* 从1开始显示的协议槽位序号。 */
    uint16_t valid_number = 0; /* 已打印有效协议的连续计数。 */

    /* count为0时使用默认打印数量10。 */
    if(count == 0) {
        count = 10;
    }

    /* 请求数量超过协议库容量时限制为全部100条。 */
    if(count > INVERTER_PROTOCOL_LIBRARY_COUNT) {
        count = INVERTER_PROTOCOL_LIBRARY_COUNT;
    }

    rt_kprintf("[%08d] Protocol library: print count=%d, capacity=%d\n", rt_tick_get(), count, INVERTER_PROTOCOL_LIBRARY_COUNT);

    /* 按1开始的协议序号依次打印有效或无效状态。 */
    for(proto_number = 1; proto_number <= count; ++proto_number) {
        const Inv_Proto_t *proto = Inv_Proto_Get(proto_number); /* 将显示序号转换为协议数组地址。 */

        /* 有效协议打印完整内容并累计有效协议序号。 */
        if(g_inv_proto_lib.valid[proto_number - 1] == INVERTER_PROTOCOL_VALID) {
            ++valid_number;
            inv_proto_print_one(proto_number, valid_number, proto);
        }
        /* 无效协议只打印槽位号和无效状态。 */
        else {
            rt_kprintf("[%08d] protocol[%d] INVALID\n", rt_tick_get(), proto_number);
        }
    }

    rt_kprintf("[%08d] Printed %d protocol slots, valid count=%d\n", rt_tick_get(), count, valid_number);
}

/* MSH命令入口，无参数打印10条，有参数时打印指定数量的协议。 */
static int inv_proto_print(int argc, char **argv)
{
    char *end_ptr;                                        /* strtol停止解析的位置，用于检查尾随字符。 */
    int32_t count = 10;                                   /* 未提供参数时默认打印的协议槽位数。 */
    int32_t max_count = INVERTER_PROTOCOL_LIBRARY_COUNT;  /* 命令参数允许的最大槽位数。 */
    int64_t parsed_count;                                 /* 命令行文本转换后的临时整数。 */

    /* 参数多于一个时打印正确用法并结束命令。 */
    if(argc > 2) {
        rt_kprintf("[%08d] usage: inv_proto_print [count:1-100]\n", rt_tick_get());
        return -1;
    }

    /* 提供数量参数时将字符串转换为整数并检查范围。 */
    if(argc == 2) {
        parsed_count = strtol(argv[1], &end_ptr, 10); /* 按十进制解析用户输入并保留结束位置。 */

        /* 参数必须是完整十进制数字，并且打印数量必须位于1～100范围内。 */
        if((end_ptr == argv[1]) || (*end_ptr != '\0') ||
           (parsed_count < 1) || (parsed_count > max_count)) {
            rt_kprintf("[%08d] invalid count, expected 1-100\n", rt_tick_get());
            return -1;
        }

        count = (int32_t)parsed_count;
    }

    Inv_Proto_Print((uint16_t)count); /* 参数检查完成后调用统一协议打印接口。 */
    return 0;
}
MSH_CMD_EXPORT(inv_proto_print, print inverter protocol library);
