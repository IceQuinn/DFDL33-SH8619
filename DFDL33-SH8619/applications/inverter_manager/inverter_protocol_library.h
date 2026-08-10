/* Copyright (c) 2026 SPDX-License-Identifier: Apache-2.0 */

#ifndef APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H
#define APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H

#include <stdint.h>

#include "user_comm.h"
#include "inverter_archive.h"
#include "meas_cfg.h"
#include "AB_check.h"

#ifdef __cplusplus
extern "C" {
#endif


/* 逆变器协议库固定提供100条厂家协议配置。 */
#define INVERTER_PROTOCOL_LIBRARY_COUNT             100U
#define INVERTER_PROTOCOL_DEFAULT_COUNT             4U
#define INVERTER_PROTOCOL_LIBRARY_VERSION           7U
#define INVERTER_PROTOCOL_INVALID                   0U
#define INVERTER_PROTOCOL_VALID                     1U


/* 使用 0xFFFF 表示某个厂家不支持或未配置该寄存器。 */
#define INVERTER_PROTOCOL_REGISTER_UNUSED           UINT16_C(0xFFFF)

/* Modbus寄存器的数据解析类型统一使用user_comm.h中的enum data_type；读取的寄存器数量由数据类型和对应厂家的协议处理逻辑确定。 */
/* 多字节数据排列方式。
 * 字母表示数据从最高有效字节到最低有效字节的正常顺序。例如32位数值的
 * 标准大端顺序为ABCD；CDAB表示两个16位寄存器交换；BADC表示每个寄存器
 * 内的两个字节交换；DCBA表示所有字节完全反序。 */
typedef enum Inv_ByteOrder
{
    /* 16位数据表示AB，32位数据表示ABCD。 */
    INVERTER_BYTE_ORDER_NORMAL = 0U,

    /* 16位数据表示BA，32位数据表示CDAB。 */
    INVERTER_BYTE_ORDER_SWAP = 1U,

    /* 仅用于32位数据，表示BADC。 */
    INVERTER_BYTE_ORDER_BADC = 2U,

    /* 仅用于32位数据，表示DCBA。 */
    INVERTER_BYTE_ORDER_DCBA = 3U
} Inv_ByteOrder_t;

/* 数据类和参数类使用的只读Modbus RTU寄存器描述，不保存实时数据。 */
#pragma pack(1)
typedef struct Inv_RegBlk
{
    /* Modbus RTU请求报文中直接使用的16位寄存器地址，不保存40001等其他表示形式，也不在使用时执行地址转换。 */
    uint16_t reg_addr;

    /* Modbus RTU读寄存器功能码，例如0x03表示读保持寄存器、0x04表示读输入寄存器；0表示未配置读功能。 */
    uint8_t read_func_code;

    /* 数据类型、字节序和小数位数共同占用一个字节。 */
    union
    {
        /* 用于存储、打印和645协议传输的完整原始字节。 */
        uint8_t raw;

        struct
        {
            /* 数据类型占bit0～bit3，使用user_comm.h中的TYPE_*值，取值范围0～15。 */
            uint8_t data_type : 4;

            /* 字节序占bit4～bit5，使用Inv_ByteOrder_t，取值范围0～3。 */
            uint8_t byte_order : 2;

            /* 小数位数占bit6～bit7，取值范围0～3。 */
            uint8_t decimal_places : 2;
        };
    };
} Inv_RegBlk_t;

/* 只读寄存器块由地址2字节、读功能码1字节和数据格式1字节组成，共4字节。 */
#define INV_REG_BLK_SIZE                            4U
typedef char Inv_RegBlkSizeCheck_t[
    (sizeof(Inv_RegBlk_t) == INV_REG_BLK_SIZE) ? 1 : -1];

/* 控制类使用的可读写Modbus RTU寄存器描述，并保存执行控制时使用的4字节默认写入值。 */
typedef struct Inv_CtrlRegBlk
{
    /* Modbus RTU请求报文中直接使用的16位寄存器地址，不保存其他地址表示形式。 */
    uint16_t reg_addr;

    /* Modbus RTU读寄存器功能码；0表示该控制点不支持或未配置读取。 */
    uint8_t read_func_code;

    /* Modbus RTU写寄存器功能码，例如0x06表示写单个寄存器、0x10表示写多个寄存器；0表示未配置写入。 */
    uint8_t write_func_code;

    /* 数据类型、字节序和小数位数共同占用一个字节。 */
    union
    {
        /* 用于存储、打印和645协议传输的完整原始字节。 */
        uint8_t raw;

        struct
        {
            /* 数据类型占bit0～bit3，取值范围0～15。 */
            uint8_t data_type : 4;

            /* 字节序占bit4～bit5，取值范围0～3。 */
            uint8_t byte_order : 2;

            /* 小数位数占bit6～bit7，取值范围0～3。 */
            uint8_t decimal_places : 2;
        };
    };

    /* 执行控制时使用的默认原始写入值，固定占用4字节；发送前按照data_type和byte_order转换。 */
    uint32_t write_default_val;
} Inv_CtrlRegBlk_t;

/* 控制寄存器块由地址2字节、读写功能码2字节、数据格式1字节和默认写入值4字节组成，共9字节。 */
#define INV_CTRL_REG_BLK_SIZE                       9U
typedef char Inv_CtrlRegBlkSizeCheck_t[
    (sizeof(Inv_CtrlRegBlk_t) == INV_CTRL_REG_BLK_SIZE) ? 1 : -1];

/* 逆变器识别使用的特征数据，描述特征寄存器的读取和解析方式以及期望特征值。 */
typedef struct Inv_Feature
{
    /* Modbus RTU请求报文中直接使用的16位特征寄存器起始地址。 */
    uint16_t reg_addr;

    /* 连续读取的16位Modbus寄存器个数。 */
    uint16_t reg_cnt;

    /* 数据类型、字节序和小数位数共同占用一个字节。 */
    union
    {
        /* 用于存储、打印和645协议传输的完整原始字节。 */
        uint8_t raw;

        struct
        {
            /* 数据类型占bit0～bit3，取值范围0～15。 */
            uint8_t data_type : 4;

            /* 字节序占bit4～bit5，取值范围0～3。 */
            uint8_t byte_order : 2;

            /* 小数位数占bit6～bit7，取值范围0～3。 */
            uint8_t decimal_places : 2;
        };
    };

    /* 读取结果转换后用于厂家及型号识别的32位特征值。 */
    uint32_t feature_val;
} Inv_Feature_t;

/* 特征数据由地址2字节、寄存器个数2字节、数据格式1字节和特征值4字节组成，共9字节。 */
#define INV_FEATURE_SIZE                            9U
typedef char Inv_FeatureSizeCheck_t[
    (sizeof(Inv_Feature_t) == INV_FEATURE_SIZE) ? 1 : -1];


/* 数据类：运行过程中周期采集的只读数据。*/
typedef struct Inv_ProtoData
{
    /* A、B、C三相电压寄存器，每相独立配置Modbus RTU寄存器地址和读功能码。 */
    Inv_RegBlk_t Ux[ENUM_PHASE_MAX];

    /* A、B、C三相电流寄存器，每相独立配置Modbus RTU寄存器地址和读功能码。 */
    Inv_RegBlk_t Ix[ENUM_PHASE_MAX];

    /* A、B、C三相有功功率寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t Px[ENUM_PHASE_MAX];

    /* A、B、C三相无功功率寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t Qx[ENUM_PHASE_MAX];

    /* A、B、C三相功率因数寄存器，每相独立配置，数组下标使用ENUM_PHASE_A/B/C。 */
    Inv_RegBlk_t PFx[ENUM_PHASE_MAX];

} Inv_ProtoData_t;


/* 当前字段布局在1字节对齐下应固定占用60字节：
 * 三相电压、三相电流               3×4×2 = 24字节
 * 三相有功、三相无功、三相功率因数 3×4×3 = 36字节
 * 合计60字节
 * 使用C99兼容的负数组长度方式执行编译期检查；后续增删字段却没有同步更新
 * 期望大小时，编译器会直接报错。 */
#define INV_PROTO_DATA_SIZE                         60U
typedef char Inv_ProtoDataSizeCheck_t[
    (sizeof(Inv_ProtoData_t) == INV_PROTO_DATA_SIZE) ? 1 : -1];

/* 参数类：逆变器的只读基础参数。 本需求将参数类定义为只读。如果某厂家允许修改设定电压，应另外在控制类 中增加对应写入点，不能直接改变参数类的访问语义。 */
typedef struct Inv_ProtoParam
{
    /* 设备编号或序列号，可能是整数、BCD或ASCII字符串。 */
    Inv_RegBlk_t dev_no;

    /* PV额定有功功率。 */
    Inv_RegBlk_t pv_rated_active_pwr;

    /* PV额定无功功率。 */
    Inv_RegBlk_t pv_rated_reactive_pwr;

    /* 逆变器设定电压。 */
    Inv_RegBlk_t set_volt;

    /* 输出类型，例如单相、三相三线或三相四线，通常为枚举或位域。 */
    Inv_RegBlk_t output_type;

    /* 开关机状态 */
    Inv_RegBlk_t pwr_status;
} Inv_ProtoParam_t;

/* 参数类包含6个只读寄存器块，按1字节对齐后共4×6=24字节。 */
#define INV_PROTO_PARAM_SIZE                        24U
typedef char Inv_ProtoParamSizeCheck_t[
    (sizeof(Inv_ProtoParam_t) == INV_PROTO_PARAM_SIZE) ? 1 : -1];

/* 控制类：允许读取当前值并写入控制命令的寄存器。
 * 开机和关机可能共用同一个地址但写入值不同，也可能使用不同寄存器，因此
 * 使用两个独立的固定命令控制点。控制百分比及功率值按各自
 * decimal_places 转换。 */
typedef struct Inv_ProtoCtrl
{
    /* 逆变器开机控制寄存器。 */
    Inv_CtrlRegBlk_t pwr_on;

    /* 逆变器关机控制寄存器。 */
    Inv_CtrlRegBlk_t pwr_off;

    /* 有功功率数值控制寄存器。 */
    Inv_CtrlRegBlk_t active_pwr_ctrl;

    /* 无功功率数值控制寄存器。 */
    Inv_CtrlRegBlk_t reactive_pwr_ctrl;
    
    /* 功率因数控制寄存器。 */
    Inv_CtrlRegBlk_t pwr_factor_ctrl;

    /* 有功功率百分比控制寄存器。 */
    Inv_CtrlRegBlk_t active_pwr_pct_ctrl;

    /* 无功功率百分比控制寄存器。 */
    Inv_CtrlRegBlk_t reactive_pwr_pct_ctrl;
} Inv_ProtoCtrl_t;

/* 控制类包含7个控制寄存器块，按1字节对齐后共9×7=63字节。 */
#define INV_PROTO_CTRL_SIZE                         63U
typedef char Inv_ProtoCtrlSizeCheck_t[
    (sizeof(Inv_ProtoCtrl_t) == INV_PROTO_CTRL_SIZE) ? 1 : -1];

/* 一条完整逆变器协议配置，包含数据类、参数类和控制类。 */
typedef struct Inv_Proto
{
    /* 1表示本条协议有效，0表示本条协议未配置。 */
    uint8_t valid;

    /* 与逆变器档案共用的厂家名称及规约版本。 */
    Inv_MfrInfo_t mfr_info;

    /* 上电识别逆变器厂家及协议时读取和比较的特征数据。 */
    Inv_Feature_t feature;

    /* 周期采集的只读运行数据。 */
    Inv_ProtoData_t data;

    /* 只读基础参数。 */
    Inv_ProtoParam_t param;

    /* 控制类。 */
    Inv_ProtoCtrl_t ctrl;
} Inv_Proto_t;

typedef struct Inv_ProtoLib
{
    /* AB区校验头，仅描述整个协议库，不属于某一条厂家协议。 */
    rcd_head head;

    /* 固定100条协议，每条协议内部包含自身的有效标志。 */
    Inv_Proto_t proto[INVERTER_PROTOCOL_LIBRARY_COUNT];
} Inv_ProtoLib_t;
#pragma pack()

/* 有效标志1字节、厂家信息34字节、特征数据9字节、数据类60字节、参数类24字节、控制类63字节，共191字节。 */
#define INV_PROTO_SIZE                              191U
typedef char Inv_ProtoSizeCheck_t[
    (sizeof(Inv_Proto_t) == INV_PROTO_SIZE) ? 1 : -1];

/* AB头6字节、100条191字节协议，共6+191×100=19106字节。 */
#define INV_PROTO_LIB_SIZE                          19106U
typedef char Inv_ProtoLibSizeCheck_t[
    (sizeof(Inv_ProtoLib_t) == INV_PROTO_LIB_SIZE) ? 1 : -1];

/* 全局逆变器协议库，数组下标范围为0~99，前4项分别对应阳光、华为、固德威和锦浪。 */
extern Inv_ProtoLib_t g_inv_proto_lib;

/* 按1~100的序号获取协议；有效和无效协议均返回对应指针，越界返回NULL。 */
const Inv_Proto_t *Inv_Proto_Get(uint16_t proto_number);

/* 打印前count条协议的全部内容；count为0时默认打印10条，最大打印100条。 */
void Inv_Proto_Print(uint16_t count);

/* 从Flash加载协议库，数据无效或版本不匹配时恢复4条默认协议。 */
void Inv_Proto_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_INVERTER_PROTOCOL_LIBRARY_H */
