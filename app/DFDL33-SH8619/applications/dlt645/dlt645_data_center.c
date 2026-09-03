#include "dlt645_data_center.h"
#include "dlt645_data_api.h"
#include <rtthread.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "dlt645_define.h"
#include "ctu_cfg.h"
#include "user_comm.h"
#include "dlt645_deal.h"
#include "user_rtc.h"
#include "main_uart.h"

#include "inv_data.h"

#define DBG_TAG "645_data"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>


uint8_t g_packBuf[256] = {0};
uint8_t sg_dl645_addr_bcd[DL645_ADDR_SIZE] = {0};

/* 数据块（表） */
const ReadBlockDataTypeDef ReadBlockDataStruct[] =
{
    {0x0400040F, 4, 8, {&ctu_cfg.longitude, &ctu_cfg.latitude, RT_NULL},       1, TYPE_U32, "long lat"},//经纬度
};

/* 单个可读可写数据（表） */
const R_WDataTypeDef R_WDataStruct[] =
{
    {0x04E60401, RT_NULL,                    1,      TYPE_U16,   4, _RW, 1, 1200, INV_CONTROL_POWER_ON, "INV_PWR_STA"},//逆变器1控制
    // {0x04E60402, RT_NULL,                    1,      TYPE_U16,   4, _RW, 1, 1200, INV_CONTROL_POWER_ON, "INV_PWR_STA"},//逆变器控制
    // {0x04000309, &relay_cfg.zero_ct_rate,               1,      TYPE_U16,   3, _RW, 1, 2000, RELAY_CFG_SRC, "zero_ct_rate"},//零序电流互感器变比
    // {0x0400030A, &ctu_cfg.show_ctrl,                    1,      TYPE_U16,   1, _RW, 1, 2,    CTU_CFG_SRC, "show ctrl"},//一二次值显示
};

#define DLT645_DATA_ID_LEN              4U  /* DL/T 645-2007数据标识DI0～DI3固定占4字节。 */
#define DLT645_WRITE_SECURITY_LEN       8U  /* 写数据命令中密码和操作者代码各占4字节。 */
#define DLT645_POINT_DATA_MAX_LEN       (D07_DATA_MAX_NR - DLT645_DATA_ID_LEN) /* 单帧扣除数据标识后的最大业务数据长度。 */

/* 645处理由串口管理线程串行调用，使用静态数据区避免在接收线程栈上放置252字节数组。 */
static uint8_t g_dlt645_point_data[DLT645_POINT_DATA_MAX_LEN]; /* 保存读取回调生成的未加0x33业务数据。 */

/* 统一点表集中描述变量类和已经迁移的运行状态点，业务取数函数统一位于dlt645_data_api.c。 */
static const Dlt645PointTypeDef g_dlt645_points[] =
{
    //DI3～DI1    DI0           读写权限            编码方式            DI0选择器                                    数据长度 定点数倍率 写入值下限 写入值上限 读取回调函数 写入回调函数 点表名称
    // 额定有功功率Pn
    {0x04E60100U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_BCD,    DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 4U,  10000, 0, 0, dlt645_read_Pn,             RT_NULL, "inverter Pn"}, /* DI0支持01～0C单台及FF全部，数据格式为XXXX.XXXX kW。 */
    // 额定无功功率Qn
    {0x04E60200U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_BCD,    DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 4U,  10000, 0, 0, dlt645_read_Qn,             RT_NULL, "inverter Qn"}, /* DI0支持01～0C单台及FF全部，数据格式为XXXX.XXXX kvar。 */
    // 三相电压数据库块
    {0x02E60100U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_BCD,    DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 6U,  10,    0, 0, dlt645_read_voltage,        RT_NULL, "inverter voltage"}, /* DI0支持01～0C单台及FF全部，数据格式为3×XXX.X。 */
    // 三相电流数据库块
    {0x02E60200U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_BCD,    DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 9U,  1000,   0, 0, dlt645_read_current,        RT_NULL, "inverter current"}, /* DI0支持01～0C单台及FF全部，数据格式为3×XXXX.XX。 */
    // 有功功率数据库块
    {0x02E60300U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_SBCD,   DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 16U, 10000, 0, 0, dlt645_read_active_power,   RT_NULL, "inverter active power"}, /* 数据按总、A、B、C排列，最高位表示符号。 */
    // 无功功率数据库块
    {0x02E60400U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_SBCD,   DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 16U, 10000, 0, 0, dlt645_read_reactive_power, RT_NULL, "inverter reactive power"}, /* 数据按总、A、B、C排列，最高位表示符号。 */
    // 功率因数数据库块
    {0x02E60500U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_BCD,    DLT645_SELECTOR_DEVICE | DLT645_SELECTOR_ALL, 8U,  1000,  0, 0, dlt645_read_power_factor,   RT_NULL, "inverter power factor"}, /* 数据按总、A、B、C排列，格式为4×X.XXX。 */
    // 全部变量数据库块
    {0x02E60F00U, 0xFFFFFF00U, DLT645_ACCESS_READ, DLT645_CODEC_CUSTOM, DLT645_SELECTOR_DEVICE,                       55U, 1,     0, 0, dlt645_read_all_variables,  RT_NULL, "inverter all variables"}, /* 规范仅允许01～0C，不接受DI0=FF聚合读取。 */
    {
        0x04E60400U,                                  /* DI3～DI1为04E604，DI0由请求选择逆变器。 */
        0xFFFFFF00U,                                  /* 忽略DI0后匹配同一类12台逆变器运行状态点。 */
        DLT645_ACCESS_READ | DLT645_ACCESS_WRITE,     /* 规范允许读取和写入运行状态。 */
        DLT645_CODEC_BCD,                             /* 运行状态使用一字节无符号BCD传输。 */
        DLT645_SELECTOR_DEVICE,                       /* 仅接受DI0为01～0C，不接受FF聚合写入。 */
        1U,                                           /* 实际运行状态数据固定占1字节。 */
        1,                                            /* 状态是枚举值，不需要小数倍率换算。 */
        0,                                            /* 规范定义写入值最小为0，即开机。 */
        1,                                            /* 规范定义写入值最大为1，即关机。 */
        dlt645_read_run_state,                        /* 读取时从g_inv_data取得推导后的运行状态。 */
        dlt645_write_run_state,                       /* 写入时提交开机或关机控制请求。 */
        "inverter run state",                        /* 日志中显示的点表名称。 */
    },
};

// const ReadDataTypeDef ReadDataStruct[] = 
// {
//     {0x0400040F, &ctu_cfg.longitude, 1.0, TYPE_U32, 4, "longitude"},//经度
//     {0x0400040F, &ctu_cfg.latitude, 1.0, TYPE_U32, 4, "latitude"},//纬度
// };

uint8_t BCD2DEC(uint8_t ch)
{
    return (ch & 0X0F) + ((ch & 0XF0) >> 4) * 10;
}

/* 大端bcd码转十进制，例如 00 00 06 00转 成600 */
int32_t BCD_Buf_To_DEC(uint8_t *inbuf, uint8_t len)
{
    int32_t val = 0;
    for(uint8_t i=len; i>0; i--)
    {
        val += BCD2DEC(inbuf[i-1]) * pow(100, len-i);
    }
    return val;
}

/* 小端bcd码转十进制，例如 00 00 06 00转 成60000 */
int32_t Little_BCD_Buf_To_DEC(uint8_t *inbuf, uint8_t len)
{
    int32_t val = 0;
    for(uint8_t i=0; i<len; i++)
    {
        val += BCD2DEC(inbuf[i]) * pow(100, i);
    }
    return val;
}

/*************************************************
Function:       str2bcd
Description:    将长度为len的字符串pstr将为BCD码输出pbcd
Calls:
Called By:
Input:

Output:

Return:
Others:       pstr的长度是pbcd的二倍

*************************************************/
void d07_str2bcd(const char *pstr, uint8_t *pbcd, int32_t len)
{
    uint8_t tmpValue;
    int32_t i;
    int32_t j;
    int32_t m;
    int32_t sLen;

    sLen = strlen(pstr);
    for(i = 0; i < sLen; i++)
    {
        if((pstr[i] < '0')
            ||((pstr[i] > '9') && (pstr[i] < 'A'))
            ||((pstr[i] > 'F') && (pstr[i] < 'a'))
            ||(pstr[i] > 'f'))
        {
            sLen=i;
            break;
        }
    }

    sLen = (sLen <= (len * 2) ) ?  sLen : sLen * 2;
    memset((void *)pbcd, 0x00, len);

    for(i=sLen-1, j=0, m=0; (i>=0)&&(m<len); i--, j++)
    {
        if((pstr[i] >= '0') && (pstr[i] <= '9'))
        {
            tmpValue=pstr[i] - '0';
        }
        else if((pstr[i] >= 'A') && (pstr[i] <= 'F'))
        {
            tmpValue=pstr[i] - 'A' + 0x0A;
        }
        else if((pstr[i] >= 'a') && (pstr[i] <= 'f'))
        {
            tmpValue=pstr[i] - 'a' + 0x0A;
        }
        else
        {
            tmpValue=0;
        }

        if((j%2)==0)
        {
            pbcd[m]=tmpValue;
        }
        else
        {
            pbcd[m++]|=(tmpValue << 4);
        }

        if((tmpValue==0) && (pstr[i] != '0'))
        {
            break;
        }
    }
}

void dl645_BufToBCD(int32_t indata, uint8_t inbuff_len, uint8_t* outbuff)
{
    if (RT_NULL == outbuff)
    {
        return;
    }
    uint8_t num_type = POSITIVE_NUM;
    if(indata < 0)
    {
        num_type = NEGATIVE_NUM;
    }
    indata = (int32_t)abs((int)indata);

    char power_str[20];

    uint8_t sprintf_format[6] = {0};
    sprintf((char *)sprintf_format, "%%0%dd", inbuff_len*2);
    sprintf((char *)power_str, (char *)sprintf_format, indata);
    d07_str2bcd(power_str, outbuff, inbuff_len);
    if(num_type == NEGATIVE_NUM)
    {   //值为负数则最高位设置为1
        outbuff[inbuff_len-1] |= 0x80;
    }

//    show_arr("set 645 bcd power", tmp_power_bcd, sizeof(tmp_power_bcd));

}

/* 645协议基础报文 */
rt_err_t dlt645_pack_base_start(uint8_t *buf, uint8_t fun_c, uint32_t id, uint16_t fun_len, uint32_t *len)
{
    /* 先导字节 */
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    /* 帧起始符 */
    buf[(*len)++] = 0x68;
    int i;
    /* 地址域 */
    for (i=0;i<6;i++)
    {
        buf[(*len)++] = sg_dl645_addr_bcd[i];
    }
    /* 帧起始符 */
    buf[(*len)++] = 0x68;
    /* 控制码 */
    buf[(*len)++] = fun_c | 0x80;

    if(E_D07_CTRL_WRITE_DATA == fun_c)
    {
        /* 写数据恢复报文长度为0 */
        buf[(*len)++] = 0;
    }
    else if (E_D07_CTRL_READ_DATA == fun_c)
    {
        /* 数据域长度(包含标识符) */
        buf[(*len)++] = 0x04+fun_len;

        /* 数据域 */
        buf[(*len)++] = (uint8_t)id + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 8) + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 16) + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 24) + 0x33;
    }
    return RT_EOK;
}

/* 645协议基础报文 */
rt_err_t dlt645_pack_base_start_w(uint8_t *buf, uint8_t fun_c, uint32_t id, uint16_t fun_len, uint32_t *len, uint8_t err_code)
{
    /* 先导字节 */
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    buf[(*len)++] = 0xFE;
    /* 帧起始符 */
    buf[(*len)++] = 0x68;
    int i;
    /* 地址域 */
    for (i=0;i<6;i++)
    {
        buf[(*len)++] = sg_dl645_addr_bcd[i];
    }
    /* 帧起始符 */
    buf[(*len)++] = 0x68;
    /* 控制码 */
    if(E_D07_W_OK == err_code){
        buf[(*len)++] = fun_c | 0x80;
    }
    else {
        buf[(*len)++] = fun_c | 0xC0;
    }

    if(E_D07_CTRL_WRITE_DATA == fun_c)
    {
        if(E_D07_W_OK == err_code)
        {
            buf[(*len)++] = 0x00;
        }
        else
        {
            buf[(*len)++] = 0x01;
            buf[(*len)++] = err_code + 0x33;
        }
    }
    else if (E_D07_CTRL_READ_DATA == fun_c)
    {
        /* 数据域长度(包含标识符) */
        buf[(*len)++] = 0x04+fun_len;

        /* 数据域 */
        buf[(*len)++] = (uint8_t)id + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 8) + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 16) + 0x33;
        buf[(*len)++] = (uint8_t)(id >> 24) + 0x33;
    }
    return RT_EOK;
}

/* 645协议基础报文 */
rt_err_t dlt645_pack_base_end(uint8_t *buf, uint32_t *len)
{
    uint8_t  cs = 0;
    for (uint8_t i=4; i<*len; i++)
    {
        cs += buf[i];
    }

    /* 校验码 */
    buf[(*len)++] = cs;
    /* 结束符 */
    buf[(*len)++] = 0x16;

    return RT_EOK;
}

/* 按点表掩码查找请求数据标识，找到时返回静态描述项地址，未找到时返回RT_NULL。 */
static const Dlt645PointTypeDef *dlt645_point_find(uint32_t id)
{
    uint16_t i; /* 当前遍历的统一点表下标。 */

    for(i = 0U; i < countof(g_dlt645_points); ++i)
    {
        const Dlt645PointTypeDef *point = &g_dlt645_points[i]; /* 当前参与掩码匹配的点描述。 */
        if((id & point->mask) == (point->id & point->mask)) /* 请求标识的有效位与点表基准标识一致。 */
        {
            return point;
        }
    }

    return RT_NULL;
}

/* 根据点描述检查DI0选择器，固定点直接通过，设备点仅允许01～0C，聚合点可允许FF。 */
static rt_bool_t dlt645_point_selector_valid(const Dlt645PointTypeDef *point, uint32_t id)
{
    uint8_t selector = (uint8_t)id; /* uint32_t数据标识最低字节对应协议中的DI0。 */

    if(point->selector == DLT645_SELECTOR_NONE) /* 固定数据标识不需要解释或限制DI0。 */
    {
        return RT_TRUE;
    }
    if(((point->selector & DLT645_SELECTOR_DEVICE) != 0U) &&
       (selector >= 1U) && (selector <= INVERTER_ARCHIVE_MAX_COUNT)) /* 01～0C分别映射12个档案槽位。 */
    {
        return RT_TRUE;
    }
    if(((point->selector & DLT645_SELECTOR_ALL) != 0U) && (selector == 0xFFU)) /* 仅声明聚合能力的点接受FF。 */
    {
        return RT_TRUE;
    }

    return RT_FALSE;
}

/* 把读取回调生成的纯数据域统一加0x33、补齐帧头校验和结束符并提交指定串口。 */
static rt_err_t dlt645_send_read_response(uint32_t id,
                                          const uint8_t *data,
                                          uint16_t data_len,
                                          uint8_t uart_no)
{
    uint32_t packlen = 0U; /* 当前应答帧已经写入g_packBuf的字节数。 */
    uint16_t index;        /* 当前复制并加0x33的业务数据下标。 */

    dlt645_pack_base_start(g_packBuf, E_D07_CTRL_READ_DATA, id, data_len, &packlen); /* 生成读应答帧头及已加0x33的数据标识。 */
    for(index = 0U; index < data_len; ++index)
    {
        g_packBuf[packlen++] = data[index] + 0x33U; /* DL/T 645要求数据域每个字节发送前加0x33。 */
    }
    dlt645_pack_base_end(g_packBuf, &packlen); /* 根据完整帧内容计算CS并追加0x16。 */
    show_arr("dlt645 tx : ", g_packBuf, packlen); /* 发送前输出完整十六进制报文便于联调。 */

    return (uart_mgmt_write(uart_no, g_packBuf, packlen) == packlen) ? RT_EOK : -RT_ERROR; /* 仅完整提交全部字节才返回成功。 */
}

/* 根据处理结果统一生成正常或异常状态应答，当前主要用于写数据命令的最终回复。 */
static rt_err_t dlt645_send_status_response(uint8_t fun_c, uint8_t err_code, uint8_t uart_no)
{
    uint32_t packlen = 0U; /* 当前状态应答帧已经写入g_packBuf的字节数。 */

    dlt645_pack_base_start_w(g_packBuf, fun_c, 0U, 0U, &packlen, err_code); /* 根据错误码选择正常控制码或异常控制码。 */
    dlt645_pack_base_end(g_packBuf, &packlen); /* 计算校验和并追加结束符。 */
    show_arr("dlt645 tx : ", g_packBuf, packlen); /* 输出最终应答报文用于确认长度字段和错误码。 */

    return (uart_mgmt_write(uart_no, g_packBuf, packlen) == packlen) ? RT_EOK : -RT_ERROR; /* 串口完整接收待发送数据才视为成功。 */
}

/* 块数据请求函数 */
int dltl645_block_bcd_data_ack(const ReadBlockDataTypeDef *PReadBlockDate, uint8_t fun_c, uint8_t *p_buf, uint16_t len, uint8_t uart_no)
{
    uint8_t dl645_power_bcd[64] = {0};
    uint32_t packlen = 0;

    dlt645_pack_base_start(g_packBuf, fun_c, PReadBlockDate->DataIdf, PReadBlockDate->AllByteLen, &packlen);

    if(E_D07_CTRL_WRITE_DATA == fun_c)
    {
        for(uint8_t i=0; i<len; ++i)
        {
            //传输时数据是反的，这里就反过来
            dl645_power_bcd[len-i-1] = p_buf[i];
        }
        for(uint8_t i=0; i<PReadBlockDate->AllByteLen; i=i+PReadBlockDate->ByteLen)
        {
            int32_t DataVal = 0;
            DataVal = BCD_Buf_To_DEC(&dl645_power_bcd[i], PReadBlockDate->ByteLen);
            SetDataFromAddr(PReadBlockDate->DataType, PReadBlockDate->ValArr[i/PReadBlockDate->ByteLen], DataVal);
        }
    }
    else if (E_D07_CTRL_READ_DATA == fun_c)
    {
        uint8_t i=0;
        /* 取数据数组的首地址 */
        uint32_t **PVal = (uint32_t **)&PReadBlockDate->ValArr[0];
        for(i=0; i<PReadBlockDate->AllByteLen; i=i+PReadBlockDate->ByteLen)
        {
            int32_t DataVal = 0;
            /* 如果该值存在，那就取地址上的数据 */
            if(*PVal != RT_NULL)
            {
                DataVal = GetDataFromAddr(PReadBlockDate->DataType, *PVal);
                //memcpy(&DataVal, *PVal, PReadBlockDate->ByteLen);
            }
            DataVal = DataVal * PReadBlockDate->DateRate;
            /* 取到了就存在buf里,没取到就存0 */
            dl645_BufToBCD(DataVal, PReadBlockDate->ByteLen, &dl645_power_bcd[i]);
            /* 准备取下一个数据 */
            PVal++;
        }

        for (i=0; i<PReadBlockDate->AllByteLen; i++)
        {
            g_packBuf[packlen++] = dl645_power_bcd[i] + 0x33;
        }
    }

    dlt645_pack_base_end(g_packBuf, &packlen);

    show_rtc_time();
    show_arr("ctu power ack : ", g_packBuf, packlen);

    uart_mgmt_write(uart_no, g_packBuf, packlen);
    return 1;
}

rt_err_t dltl645_ymdw_ack(uint8_t fun_c, uint32_t id, uint8_t *p_buf, uint16_t len, uint8_t uart_no)
{
    uint8_t dl645_power_bcd[64] = {0};
    uint32_t packlen = 0;

    dlt645_pack_base_start(g_packBuf, fun_c, id, 4, &packlen);

    time_t now;
    now = time(RT_NULL);

    struct tm *p_tm;
    p_tm = localtime(&now);

    if(E_D07_CTRL_WRITE_DATA == fun_c)
    {
        for(uint8_t i=0; i<len; ++i)
        {
            //传输时数据是反的，这里就反过来
            dl645_power_bcd[len-i-1] = p_buf[i];
        }
        p_tm->tm_year = BCD2DEC(dl645_power_bcd[0])+100;
        p_tm->tm_mon = BCD2DEC(dl645_power_bcd[1]-1);
        p_tm->tm_mday = BCD2DEC(dl645_power_bcd[2]);
        p_tm->tm_wday = BCD2DEC(dl645_power_bcd[3]);
        now = mktime(p_tm);
        set_timestamp(now);
    }
    else if (E_D07_CTRL_READ_DATA == fun_c)
    {
        dl645_BufToBCD(p_tm->tm_year-100, 1, &dl645_power_bcd[0]);
        dl645_BufToBCD(p_tm->tm_mon+1, 1, &dl645_power_bcd[1]);
        dl645_BufToBCD(p_tm->tm_mday, 1, &dl645_power_bcd[2]);
        dl645_BufToBCD(p_tm->tm_wday, 1, &dl645_power_bcd[3]);

        uint16_t i=0;
        for (i=0; i<4; i++)
        {
            g_packBuf[packlen++] = dl645_power_bcd[4-i-1] + 0x33;
        }
    }


    dlt645_pack_base_end(g_packBuf, &packlen);

    show_rtc_time();
    show_arr("ctu power ack : ", g_packBuf, packlen);

    uart_mgmt_write(uart_no, g_packBuf, packlen);
    return RT_EOK;

}

rt_err_t dltl645_hms_ack(uint8_t fun_c, uint32_t id, uint8_t *p_buf, uint16_t len, uint8_t uart_no)
{
    uint8_t dl645_power_bcd[64] = {0};
    uint32_t packlen = 0;

    dlt645_pack_base_start(g_packBuf, fun_c, id, 3, &packlen);

    time_t now;
    now = time(RT_NULL);

    struct tm *p_tm;
    p_tm = localtime(&now);

    if(E_D07_CTRL_WRITE_DATA == fun_c)
    {
        for(uint8_t i=0; i<len; ++i)
        {
            //传输时数据是反的，这里就反过来
            dl645_power_bcd[len-i-1] = p_buf[i];
        }
        p_tm->tm_hour = BCD2DEC(dl645_power_bcd[0]);
        p_tm->tm_min = BCD2DEC(dl645_power_bcd[1]);
        p_tm->tm_sec = BCD2DEC(dl645_power_bcd[2]);
        now = mktime(p_tm);
        set_timestamp(now);
    }
    else if (E_D07_CTRL_READ_DATA == fun_c)
    {
        dl645_BufToBCD(p_tm->tm_hour, 1, &dl645_power_bcd[0]);
        dl645_BufToBCD(p_tm->tm_min, 1, &dl645_power_bcd[1]);
        dl645_BufToBCD(p_tm->tm_sec, 1, &dl645_power_bcd[2]);

        uint16_t i=0;
        for (i=0; i<3; i++)
        {
            g_packBuf[packlen++] = dl645_power_bcd[3-i-1] + 0x33;
        }
    }

    dlt645_pack_base_end(g_packBuf, &packlen);

    show_rtc_time();
    show_arr("ctu power ack : ", g_packBuf, packlen);

    uart_mgmt_write(uart_no, g_packBuf, packlen);
    return RT_EOK;
}

/* 无请求数据应答 */
int translayerdl645_no_data_requested_ack(uint8_t fun_c, uint8_t uart_no)
{
    uint32_t packLen = 0;
    uint8_t i = 0;

    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    //1 生成应答报文

    g_packBuf[packLen++] = 0x68;

    for (i=0;i<6;i++)
    {
        g_packBuf[packLen++] = sg_dl645_addr_bcd[i];
    }
    g_packBuf[packLen++] = 0x68;
    g_packBuf[packLen++] = fun_c | 0xC0;
    g_packBuf[packLen++] = 0x01;
    g_packBuf[packLen++] = 0x02+0x33;

    dlt645_pack_base_end(g_packBuf, &packLen);

    show_rtc_time();
    show_arr("ctu power ack : ", g_packBuf, packLen);

    uart_mgmt_write(uart_no, g_packBuf, packLen);
    return 1;
}

/* 查找并执行读数据标识；新点使用统一回调组帧，未迁移旧点暂时保留原读取入口。 */
void dlt645_ctrl_read_data(uint8_t fun_c, uint32_t id,  uint8_t uart_no)
{
    const Dlt645PointTypeDef *point = dlt645_point_find(id); /* 统一点表中与请求数据标识匹配的描述项。 */
    uint16_t data_len = 0U; /* 读取回调实际写入静态业务数据缓冲区的字节数。 */

    if(point != RT_NULL) /* 新点表匹配成功后不再进入旧点表兼容分支。 */
    {
        LOG_I("recv dlt645 read %s", point->name);
        if(((point->access & DLT645_ACCESS_READ) == 0U) ||
           (point->read == RT_NULL) ||
           !dlt645_point_selector_valid(point, id) ||
           (point->read(point, id, g_dlt645_point_data,
                        sizeof(g_dlt645_point_data), &data_len) != RT_EOK) ||
           (data_len > DLT645_POINT_DATA_MAX_LEN)) /* 权限、回调、DI0、取数结果和输出长度必须全部有效。 */
        {
            translayerdl645_no_data_requested_ack(E_D07_CTRL_READ_DATA, uart_no); /* 无法提供合法数据时返回无请求数据异常。 */
            return;
        }

        dlt645_send_read_response(id, g_dlt645_point_data, data_len, uart_no); /* 顶层统一执行加0x33、组帧和单次发送。 */
        return;
    }

    /* 尚未迁移到统一点表的旧数据块只保留读兼容，写入口不再调用。 */
    for(uint16_t i = 0U; i < countof(ReadBlockDataStruct); ++i)
    {
        if(id == ReadBlockDataStruct[i].DataIdf) /* 旧数据块使用完整数据标识精确匹配。 */
        {
            LOG_I("recv dlt645 %s ", ReadBlockDataStruct[i].DescribeType);
            dltl645_block_bcd_data_ack(&ReadBlockDataStruct[i], E_D07_CTRL_READ_DATA,
                                       RT_NULL, 0, uart_no); /* 旧函数内部仍负责读取、组帧和发送。 */
            return;
        }
    }

    if(id == 0x04000101) /* 日期及星期暂时通过原专用读取函数回复。 */
    {
        dltl645_ymdw_ack(E_D07_CTRL_READ_DATA, id, RT_NULL, 0, uart_no);
        return;
    }
    if(id == 0x04000102) /* 时分秒暂时通过原专用读取函数回复。 */
    {
        dltl645_hms_ack(E_D07_CTRL_READ_DATA, id, RT_NULL, 0, uart_no);
        return;
    }

    LOG_I("Dlt645 read data failed: Not support"); /* 新旧点表和专用点均未匹配时记录不支持日志。 */
    translayerdl645_no_data_requested_ack(E_D07_CTRL_READ_DATA, uart_no); /* 对未知数据标识返回无请求数据异常。 */
}

/* 查找并执行写数据标识，统一完成权限、DI0、报文长度、业务回调和最终状态应答。 */
void dlt645_ctrl_write_data(uint8_t fun_c, uint32_t id, uint8_t *p_buf, uint16_t len, uint8_t uart_no)
{
    const Dlt645PointTypeDef *point = dlt645_point_find(id); /* 统一点表中与请求数据标识匹配的描述项。 */
    uint8_t err_code = E_D07_W_NO_DATA; /* 默认按未知点或无写权限回复无请求数据。 */

    RT_UNUSED(fun_c); /* 调用入口当前固定传入写数据功能码，状态应答层直接使用协议常量。 */
    if(point != RT_NULL) /* 仅匹配到已登记点后才检查权限并调用写处理函数。 */
    {
        LOG_I("recv dlt645 write %s", point->name);
        if(((point->access & DLT645_ACCESS_WRITE) != 0U) &&
           (point->write != RT_NULL) &&
           dlt645_point_selector_valid(point, id)) /* 点必须具备写权限、有效写回调和合法DI0选择器。 */
        {
            uint16_t expected_len = DLT645_WRITE_SECURITY_LEN + point->data_len; /* p_buf应包含8字节安全字段和固定长度业务数据。 */

            /* p_buf由解析层完成减0x33，内容为密码、操作者代码和实际写数据。 */
            if((p_buf != RT_NULL) && (len == expected_len) &&
               (point->write(point, id, &p_buf[DLT645_WRITE_SECURITY_LEN],
                             point->data_len) == RT_EOK)) /* 指针、总长度和业务写入结果全部有效才正常应答。 */
            {
                err_code = E_D07_W_OK; /* 设备已返回成功结果，回复正常写数据应答。 */
            }
            else
            {
                err_code = E_D07_W_ERR; /* 报文非法或控制执行失败，回复其他错误。 */
            }
        }
    }

    dlt645_send_status_response(E_D07_CTRL_WRITE_DATA, err_code, uart_no); /* 每个写请求只在顶层发送一次最终状态。 */
}
