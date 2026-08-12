#include "inv_protocol_print.h"
#include "inverter_protocol_library.h"
#include <rtthread.h>
#include <stdlib.h>

/* 返回数据类型的可读名称，便于协议库命令行检查。 */
static const char *inv_proto_data_type_name(uint8_t data_type)
{
    switch (data_type)
    {
    case TYPE_NONE:      return "NONE";
    case TYPE_I8:        return "INT8";
    case TYPE_U8:        return "UINT8";
    case TYPE_I16:       return "INT16";
    case TYPE_U16:       return "UINT16";
    case TYPE_I32:       return "INT32";
    case TYPE_U32:       return "UINT32";
    case TYPE_FLOAT32:   return "FLOAT32";
    case TYPE_FLOAT64:   return "FLOAT64";
    case TYPE_ASCII:     return "ASCII";
    case TYPE_BCD:       return "BCD";
    case TYPE_BCD_TIME:  return "BCD_TIME";
    case TYPE_BIT_FIELD: return "BIT_FIELD";
    default:             return "UNKNOWN";
    }
}

/* 返回压缩字节序在当前数据类型下对应的可读名称。 */
static const char *inv_proto_byte_order_name(uint8_t data_type,
                                              uint8_t byte_order)
{
    switch (byte_order)
    {
    case INVERTER_BYTE_ORDER_NORMAL:
        if ((data_type == TYPE_I16) || (data_type == TYPE_U16))
        {
            return "AB";
        }
        if ((data_type == TYPE_I32) ||
            (data_type == TYPE_U32) ||
            (data_type == TYPE_FLOAT32))
        {
            return "ABCD";
        }
        return "NORMAL";

    case INVERTER_BYTE_ORDER_SWAP:
        if ((data_type == TYPE_I16) || (data_type == TYPE_U16))
        {
            return "BA";
        }
        if ((data_type == TYPE_I32) ||
            (data_type == TYPE_U32) ||
            (data_type == TYPE_FLOAT32))
        {
            return "CDAB";
        }
        return "SWAP";

    case INVERTER_BYTE_ORDER_BADC: return "BADC";
    case INVERTER_BYTE_ORDER_DCBA: return "DCBA";
    default:                        return "UNKNOWN";
    }
}

/* 使用与只读数据行相同的固定列宽打印数据类或参数类表头。 */
static void inv_proto_print_read_table_header(const char *section_name)
{
    rt_kprintf("\t%-20s %-8s %-7s %-8s %-14s %-12s %-7s\n",
               section_name,
               "addr",
               "count",
               "read_fc",
               "type",
               "order",
               "decimal");
}

/* 按只读表格中的一行打印数据类或参数类寄存器块。 */
static void inv_proto_print_reg(const char *name, const Inv_RegBlk_t *reg)
{
    rt_kprintf("\t%-20s 0x%04X   %-7u 0x%02X     %-14s %-12s %-7u\n",
               name,
               (unsigned int)reg->reg_addr,
               (unsigned int)reg->reg_cnt,
               (unsigned int)reg->read_func_code,
               inv_proto_data_type_name(reg->data_type),
               inv_proto_byte_order_name(reg->data_type, reg->byte_order),
               (unsigned int)reg->decimal_places);
}

/* 打印普通控制寄存器表头。 */
static void inv_proto_print_ctrl_table_header(const char *section_name)
{
    rt_kprintf("\t%-20s %-8s %-7s %-8s %-14s %-12s %-7s\n",
               section_name,
               "addr",
               "count",
               "write_fc",
               "type",
               "order",
               "decimal");
}

/* 打印不带默认值的普通可读写控制寄存器。 */
static void inv_proto_print_ctrl_reg(const char *name, const Inv_CtrlRegBlk_t *reg)
{
    rt_kprintf("\t%-20s 0x%04X   %-7u 0x%02X     %-14s %-12s %-7u\n",
               name,
               (unsigned int)reg->reg_addr,
               (unsigned int)reg->reg_cnt,
               (unsigned int)reg->write_func_code,
               inv_proto_data_type_name(reg->data_type),
               inv_proto_byte_order_name(reg->data_type, reg->byte_order),
               (unsigned int)reg->decimal_places);
}

/* 打印开机、关机固定命令及其默认写入值。 */
static void inv_proto_print_default_ctrl_reg(const char *name,
                                             const Inv_CtrlDefaultRegBlk_t *reg)
{
    rt_kprintf("\t%-20s 0x%04X   %-7u 0x%02X     %-14s %-12s %-7u default=0x%08X\n",
               name,
               (unsigned int)reg->reg_addr,
               (unsigned int)reg->reg_cnt,
               (unsigned int)reg->write_func_code,
               inv_proto_data_type_name(reg->data_type),
               inv_proto_byte_order_name(reg->data_type, reg->byte_order),
               (unsigned int)reg->decimal_places,
               (unsigned int)reg->write_default_val);
}

/* 打印特征寄存器地址、数量和解析格式。 */
static void inv_proto_print_feature(const Inv_Feature_t *feature)
{
    rt_kprintf("\t%-20s %-8s %-7s %-8s %-14s %-12s %-7s\n",
               "[feature]",
               "addr",
               "count",
               "read_fc",
               "type",
               "order",
               "decimal");
    rt_kprintf("\t%-20s 0x%04X   %-7u 0x%02X     %-14s %-12s %-7u\n",
               "feature",
               (unsigned int)feature->reg_addr,
               (unsigned int)feature->reg_cnt,
               (unsigned int)feature->read_func_code,
               inv_proto_data_type_name(feature->data_type),
               inv_proto_byte_order_name(feature->data_type,
                                          feature->byte_order),
               (unsigned int)feature->decimal_places);
}

/* 将固定32字节厂家名称转换为保证以NUL结束的可打印字符串。 */
static void inv_proto_copy_mfr_name(char output[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U],
                                    const char input[INVERTER_ARCHIVE_BRAND_WIRE_SIZE])
{
    uint8_t index;

    for (index = 0U; index < INVERTER_ARCHIVE_BRAND_WIRE_SIZE; ++index)
    {
        uint8_t character = (uint8_t)input[index];

        if ((character == 0U) || (character == 0xFFU))
        {
            break;
        }
        output[index] = ((character >= 0x20U) && (character <= 0x7EU))
                            ? (char)character
                            : '?';
    }
    output[index] = '\0';
}

const Inv_Proto_t *Inv_Proto_Get(uint16_t proto_number)
{
    if ((proto_number == 0U) ||
        (proto_number > INVERTER_PROTOCOL_LIBRARY_COUNT))
    {
        return RT_NULL;
    }

    return &g_inv_proto_lib.proto[proto_number - 1U];
}

/* 完整打印一条协议中的厂家信息、数据类、参数类和控制类。 */
static void inv_proto_print_one(uint16_t proto_number,
                                uint16_t valid_number,
                                const Inv_Proto_t *proto)
{
    char manufacturer[INVERTER_ARCHIVE_BRAND_WIRE_SIZE + 1U];

    inv_proto_copy_mfr_name(manufacturer, proto->mfr_info.name);
    rt_kprintf("\n[%u][%u]\t",
               (unsigned int)valid_number,
               (unsigned int)proto_number);
    rt_kprintf("manufacturer : %s", manufacturer[0] != '\0' ? manufacturer : "(empty)");
    rt_kprintf("\tprotocol ver : 0x%02X 0x%02X\n",
               (unsigned int)proto->mfr_info.proto_ver[0],
               (unsigned int)proto->mfr_info.proto_ver[1]);

    inv_proto_print_feature(&proto->feature);

    rt_kprintf("\n");
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

    rt_kprintf("\n");
    inv_proto_print_read_table_header("[parameter]");
    inv_proto_print_reg("device_no", &proto->param.dev_no);
    inv_proto_print_reg("pv_rated_p", &proto->param.pv_rated_active_pwr);
    inv_proto_print_reg("pv_rated_q", &proto->param.pv_rated_reactive_pwr);
    inv_proto_print_reg("set_voltage", &proto->param.set_volt);
    inv_proto_print_reg("output_type", &proto->param.output_type);
    inv_proto_print_reg("power_status", &proto->param.pwr_status);

    rt_kprintf("\n");
    inv_proto_print_ctrl_table_header("[control]");
    inv_proto_print_default_ctrl_reg("power_on", &proto->ctrl.pwr_on);
    inv_proto_print_default_ctrl_reg("power_off", &proto->ctrl.pwr_off);
    inv_proto_print_ctrl_reg("active_pwr_value", &proto->ctrl.active_pwr_ctrl);
    inv_proto_print_ctrl_reg("reactive_pwr_value", &proto->ctrl.reactive_pwr_ctrl);
    inv_proto_print_ctrl_reg("power_factor", &proto->ctrl.pwr_factor_ctrl);
    inv_proto_print_ctrl_reg("active_pwr_pct", &proto->ctrl.active_pwr_pct_ctrl);
    inv_proto_print_ctrl_reg("reactive_pwr_pct", &proto->ctrl.reactive_pwr_pct_ctrl);
}

void Inv_Proto_Print(uint16_t count)
{
    uint16_t proto_number;
    uint16_t valid_number = 0U;

    if (count == 0U)
    {
        count = 10U;
    }
    if (count > INVERTER_PROTOCOL_LIBRARY_COUNT)
    {
        count = INVERTER_PROTOCOL_LIBRARY_COUNT;
    }

    rt_kprintf("\nProtocol library: print count=%u, capacity=%u\n",
               (unsigned int)count,
               (unsigned int)INVERTER_PROTOCOL_LIBRARY_COUNT);
    for (proto_number = 1U; proto_number <= count; ++proto_number)
    {
        const Inv_Proto_t *proto = Inv_Proto_Get(proto_number);

        if (proto->valid == INVERTER_PROTOCOL_VALID)
        {
            ++valid_number;
            inv_proto_print_one(proto_number, valid_number, proto);
        }
        else
        {
            rt_kprintf("\n[ ][%u]\tINVALID\n", (unsigned int)proto_number);
        }
    }
    rt_kprintf("\nPrinted %u protocol slots, valid count=%u\n",
               (unsigned int)count,
               (unsigned int)valid_number);
}

/* MSH命令：无参数打印10条，有参数打印指定数量的协议。 */
static int inv_proto_print(int argc, char **argv)
{
    int count = 10;

    if (argc > 2)
    {
        rt_kprintf("usage: inv_proto_print [count:1-100]\n");
        return -1;
    }
    if (argc == 2)
    {
        count = atoi(argv[1]);
        if ((count < 1) || (count > (int)INVERTER_PROTOCOL_LIBRARY_COUNT))
        {
            rt_kprintf("invalid count, expected 1-100\n");
            return -1;
        }
    }

    Inv_Proto_Print((uint16_t)count);
    return 0;
}
MSH_CMD_EXPORT(inv_proto_print, print inverter protocol library);

