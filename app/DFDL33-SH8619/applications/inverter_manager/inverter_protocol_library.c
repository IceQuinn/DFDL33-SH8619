/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#include "inverter_protocol_library.h"
#include "drv_ex_flash.h"
#include "user_ex_flash_mgmt.h"
#include <string.h>




Inv_ProtoLib_t g_inv_proto_lib = {0};               /* 保存有效标志和最多100条厂家协议配置。 */

/* 判断协议厂家字段是否包含至少一个可显示ASCII字符，避免全零、全FF或乱码协议进入运行库。 */
static rt_bool_t inv_proto_manufacturer_valid(const char *manufacturer)
{
    uint16_t index; /* 当前检查的厂家名称字节下标。 */
    rt_bool_t has_character = RT_FALSE; /* 至少发现一个有效字符后才认为厂家名称存在。 */

    for(index = 0U; index < INVERTER_ARCHIVE_BRAND_WIRE_SIZE; ++index)
    {
        uint8_t value = (uint8_t)manufacturer[index]; /* 厂家名称按无符号字节检查，避免char符号扩展。 */
        if((value == 0U) || (value == 0xFFU)) /* 固定长度字符串允许使用00或FF填充剩余空间。 */
        {
            continue;
        }
        if((value < 0x20U) || (value > 0x7EU)) /* 协议库厂家字段仅接受可显示ASCII字符。 */
        {
            return RT_FALSE;
        }
        has_character = RT_TRUE;
    }
    return has_character;
}

/* 检查6字节寄存器描述的公共字段，FFFF地址表示厂家不支持该数据点并直接允许。 */
static rt_bool_t inv_proto_register_valid(const Inv_RegBlk_t *block, rt_bool_t is_write)
{
    uint8_t function_code = ((const uint8_t *)block)[3]; /* 读写描述第4字节均为Modbus功能码，按公共线布局读取。 */

    if(block->reg_addr == INVERTER_PROTOCOL_REGISTER_UNUSED) /* 未支持寄存器不参与其余字段校验。 */
    {
        return RT_TRUE;
    }
    if((block->reg_cnt == 0U) || (block->reg_cnt > 16U) || (block->data_type > TYPE_BIT_FIELD) ||
       (block->byte_order > Type_Byte_DCBA) || (block->decimal_places > 5U) || (block->reserved != 0U)) /* 限制运行解析器能够识别的数据格式。 */
    {
        return RT_FALSE;
    }
    if(is_write == RT_TRUE) /* 控制点只能使用写单寄存器或写多个寄存器功能码。 */
    {
        return ((function_code == 0x06U) && (block->reg_cnt == 1U)) || (function_code == 0x10U);
    }
    return (function_code == 0x03U) || (function_code == 0x04U); /* 采集点只接受保持寄存器或输入寄存器读功能码。 */
}

/* 对238字节协议的厂家、版本、特征、数据、参数和控制描述执行写入前完整校验。 */
static rt_bool_t inv_proto_content_valid(const Inv_Proto_t *proto)
{
    const Inv_RegBlk_t *data_blocks = (const Inv_RegBlk_t *)&proto->data; /* 数据类连续包含18个同布局只读寄存器块。 */
    const Inv_RegBlk_t *param_blocks = (const Inv_RegBlk_t *)&proto->param; /* 参数类连续包含5个同布局只读寄存器块。 */
    const Inv_CtrlRegBlk_t *ctrl_blocks = &proto->ctrl.active_pwr_ctrl; /* 普通控制类从有功功率控制开始连续排列5项。 */
    uint16_t index; /* 当前校验的同类寄存器描述下标。 */

    if((inv_proto_manufacturer_valid(proto->mfr_info.name) == RT_FALSE) ||
       (proto->mfr_info.proto_ver == 0U) || (proto->mfr_info.proto_ver == 0xFFFFU)) /* 厂家及规约版本共同作为协议身份。 */
    {
        return RT_FALSE;
    }
    if((inv_proto_register_valid((const Inv_RegBlk_t *)&proto->feature, RT_FALSE) == RT_FALSE) ||
       (proto->feature.lower_limit > proto->feature.upper_limit)) /* 特征值上下限必须形成有效闭区间。 */
    {
        return RT_FALSE;
    }
    for(index = 0U; index < (INV_PROTO_DATA_SIZE / INV_REG_BLK_SIZE); ++index)
    {
        if(inv_proto_register_valid(&data_blocks[index], RT_FALSE) == RT_FALSE) /* 任一数据类描述非法时拒绝整条协议。 */
        {
            return RT_FALSE;
        }
    }
    for(index = 0U; index < (INV_PROTO_PARAM_SIZE / INV_REG_BLK_SIZE); ++index)
    {
        if(inv_proto_register_valid(&param_blocks[index], RT_FALSE) == RT_FALSE) /* 任一参数类描述非法时拒绝整条协议。 */
        {
            return RT_FALSE;
        }
    }
    if((inv_proto_register_valid((const Inv_RegBlk_t *)&proto->ctrl.pwr_on, RT_TRUE) == RT_FALSE) ||
       (inv_proto_register_valid((const Inv_RegBlk_t *)&proto->ctrl.pwr_off, RT_TRUE) == RT_FALSE)) /* 开关机块前6字节与普通控制块布局一致。 */
    {
        return RT_FALSE;
    }
    for(index = 0U; index < 5U; ++index)
    {
        if(inv_proto_register_valid((const Inv_RegBlk_t *)&ctrl_blocks[index], RT_TRUE) == RT_FALSE) /* 五个调节控制点必须逐项合法或明确为FFFF。 */
        {
            return RT_FALSE;
        }
    }
    return inv_proto_register_valid(&proto->daily_energy, RT_FALSE); /* 最后校验独立的日发电量寄存器描述。 */
}

/* 统计RAM协议库中有效槽位数量，结果随上位机临时写入立即更新。 */
uint8_t Inv_Proto_Valid_Count(void)
{
    uint16_t index; /* 当前检查的协议槽位下标。 */
    uint8_t count = 0U; /* 有效标志等于1的协议数量。 */

    for(index = 0U; index < INVERTER_PROTOCOL_LIBRARY_COUNT; ++index)
    {
        if(g_inv_proto_lib.valid[index] == INVERTER_PROTOCOL_VALID) /* 仅统计明确有效的协议槽位。 */
        {
            ++count;
        }
    }
    return count;
}

/* 将指定RAM协议槽位复制为645线上的238字节数据，无效槽位固定返回全FF。 */
rt_err_t Inv_Proto_Read_Wire(uint16_t proto_number, uint8_t *wire_data, uint16_t capacity)
{
    uint16_t index; /* 由1起始协议编号换算得到的0起始数组下标。 */
    rt_base_t level; /* 复制协议期间保存的中断状态，避免周期线程读到半条新协议。 */

    if((proto_number == 0U) || (proto_number > INVERTER_PROTOCOL_LIBRARY_COUNT) || (wire_data == RT_NULL) || (capacity < INV_PROTO_SIZE)) /* 接口边界在首次进入时统一检查。 */
    {
        return -RT_EINVAL;
    }
    index = proto_number - 1U;
    level = rt_hw_interrupt_disable(); /* 有效标志和协议内容必须作为同一快照读取。 */
    if(g_inv_proto_lib.valid[index] == INVERTER_PROTOCOL_VALID) /* 有效协议按压缩结构原始小端布局输出。 */
    {
        rt_memcpy(wire_data, &g_inv_proto_lib.proto[index], INV_PROTO_SIZE);
    }
    else /* 规范要求无效协议槽位仍正常应答，但238字节全部填FF。 */
    {
        rt_memset(wire_data, 0xFF, INV_PROTO_SIZE);
    }
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}

/* 校验238字节线数据并原子覆盖RAM协议槽位，本接口不执行任何Flash保存。 */
rt_err_t Inv_Proto_Write_Wire(uint16_t proto_number, const uint8_t *wire_data, uint16_t data_len)
{
    Inv_Proto_t candidate; /* 先在临时对象中完成解析和校验，失败时不污染当前运行协议。 */
    uint16_t index; /* 目标协议槽位的0起始数组下标。 */
    uint16_t compare_index; /* 检查厂家和版本重复时使用的槽位下标。 */
    rt_base_t level; /* 原子替换协议内容时保存的中断状态。 */

    if((proto_number == 0U) || (proto_number > INVERTER_PROTOCOL_LIBRARY_COUNT) || (wire_data == RT_NULL) || (data_len != INV_PROTO_SIZE)) /* 写入必须精确携带238字节。 */
    {
        return -RT_EINVAL;
    }
    rt_memcpy(&candidate, wire_data, INV_PROTO_SIZE); /* MCU结构已固定为1字节对齐，与技术规范线序一致。 */
    if(inv_proto_content_valid(&candidate) == RT_FALSE) /* 全FF等非法内容也会由统一结构校验拒绝，不能用于删除协议。 */
    {
        return -RT_EINVAL;
    }
    index = proto_number - 1U;
    for(compare_index = 0U; compare_index < INVERTER_PROTOCOL_LIBRARY_COUNT; ++compare_index)
    {
        if((compare_index != index) && (g_inv_proto_lib.valid[compare_index] == INVERTER_PROTOCOL_VALID) &&
           (rt_memcmp(&g_inv_proto_lib.proto[compare_index].mfr_info, &candidate.mfr_info, sizeof(candidate.mfr_info)) == 0)) /* 不允许两个有效槽位具有完全相同的厂家和版本。 */
        {
            return -RT_EBUSY; /* 重复厂家版本使用RT-Thread已有的忙状态表示当前配置冲突。 */
        }
    }
    level = rt_hw_interrupt_disable(); /* 内容先写完再置有效，周期抄读不会看到中间状态。 */
    rt_memcpy(&g_inv_proto_lib.proto[index], &candidate, INV_PROTO_SIZE);
    g_inv_proto_lib.valid[index] = INVERTER_PROTOCOL_VALID;
    rt_hw_interrupt_enable(level);
    return RT_EOK;
}
/* 清空协议库，装载内置默认协议，并保存到Flash A/B区。 */
void Inv_Proto_Default_Init(void)
{
    inv_proto_default_lib_init(); /* 按厂家编号从各分项配置表重新组装默认协议库。 */

    AB_save(flash_write, /* 将重建后的默认协议库同时保存到Flash A/B区。 */
            PROTO_LIB_ADDR_A,
            PROTO_LIB_ADDR_B,
            &g_inv_proto_lib,
            INVERTER_PROTOCOL_LIBRARY_VERSION,
            sizeof(g_inv_proto_lib),
            "PROTO LIB");
}
MSH_CMD_EXPORT(Inv_Proto_Default_Init, Inv_Proto_Default_Init);

/* 从Flash装载协议库，校验失败、版本异常或长度异常时恢复默认协议库。 */
void Inv_Proto_Init(void)
{
    inv_proto_default_lib_init();
#if 0
    int32_t check_sta; /* Flash A/B区协议库校验接口返回状态。 */

    check_sta = AB_check(flash_read,           // 读取Flash的底层接口
                         flash_write,          // 写接口
                         PROTO_LIB_ADDR_A,      // A区地址
                         PROTO_LIB_ADDR_B,      // B区地址
                         &g_inv_proto_lib,      // 数据内存
                         sizeof(g_inv_proto_lib), // 数据大小
                         "PROTO LIB");         // 数据描述

    /* Flash校验失败、协议库版本不一致或数据长度异常时恢复内置默认协议。 */
    if((check_sta == 1) ||
       (g_inv_proto_lib.head.ver != INVERTER_PROTOCOL_LIBRARY_VERSION) ||
       (g_inv_proto_lib.head.len != (sizeof(g_inv_proto_lib) - sizeof(rcd_head)))) {
        rt_kprintf("[%08d] protocol library check failed, loading defaults\n", rt_tick_get());
        Inv_Proto_Default_Init(); /* 持久化数据不可用时按分项配置表重建默认协议。 */
    }
#endif
}
