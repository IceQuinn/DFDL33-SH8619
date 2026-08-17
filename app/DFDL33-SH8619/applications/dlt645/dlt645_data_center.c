#include "dlt645_data_center.h"
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

void dlt645_ctrl_read_data(uint8_t fun_c, uint32_t id,  uint8_t uart_no)
{
    uint8_t ReadData_Support_flg = 0;    /* 装置是否支持该数据标识，支持:1，不支持:0 */

    /* 基本数据块标识 */
    for(int i=0; i<countof(ReadBlockDataStruct); i++)
    {
        if(id == ReadBlockDataStruct[i].DataIdf)
        {
            LOG_I("recv dlt645 %s ", ReadBlockDataStruct[i].DescribeType);
            ReadData_Support_flg = 1;
            dltl645_block_bcd_data_ack(&ReadBlockDataStruct[i], fun_c, RT_NULL, 0, uart_no);
            break;
        }
    }

    if(id == 0x04000101)    //年月日星期
    {
        dltl645_ymdw_ack(fun_c, id, RT_NULL, 0, uart_no);
        ReadData_Support_flg = 1;
    }
    if(id == 0x04000102)    //时分秒
    {
        dltl645_hms_ack(fun_c, id, RT_NULL, 0, uart_no);
        ReadData_Support_flg = 1;
    }


    if(ReadData_Support_flg == 0)
    {
        /* 不支持该数据标识 */
        LOG_I("Dlt645 read data failed: Not support");
        translayerdl645_no_data_requested_ack(fun_c, uart_no);
    }
}

void dlt645_ctrl_write_data(uint8_t fun_c, uint32_t id, uint8_t *p_buf, uint16_t len, uint8_t uart_no)
{
    uint8_t write_data_support_flg = 0;    /* 装置是否支持该数据标识，支持:1，不支持:0 */

    /* 基本数据块标识 */
    for(int i=0; i<countof(ReadBlockDataStruct); i++)
    {
        if(id == ReadBlockDataStruct[i].DataIdf)
        {
            LOG_I("recv dlt645 %s ", ReadBlockDataStruct[i].DescribeType);
            write_data_support_flg = 1;
            dltl645_block_bcd_data_ack(&ReadBlockDataStruct[i], fun_c, p_buf, len, uart_no);
            break;
        }
    }

    if(id == 0x04000101)    //年月日星期
    {
        dltl645_ymdw_ack(fun_c, id, p_buf, len, uart_no);
        write_data_support_flg = 1;
    }
    if(id == 0x04000102)    //时分秒
    {
        dltl645_hms_ack(fun_c, id, p_buf, len, uart_no);
        write_data_support_flg = 1;
    }

    if(write_data_support_flg == 0)
    {
        /* 不支持该数据标识 */
        LOG_I("Dlt645 write data failed: Not support");
        translayerdl645_no_data_requested_ack(fun_c, uart_no);
    }
}
