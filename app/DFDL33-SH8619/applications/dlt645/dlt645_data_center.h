#ifndef __DLT645_DATA_CENTER__
#define __DLT645_DATA_CENTER__

#include <stdint.h>
#include <rtthread.h>
#include "dlt645_data_api.h"

/* 645报文下行报文结构体 */
struct dlt645_w
{
    uint8_t PA_P2[4];
    uint8_t C0_C4[4];
    uint8_t data[64];
};

typedef struct
{
    uint32_t DataIdf;       //645数据标识
    void    *DataBuff;     //回复数据地址
    float    DateRate;      //上送的数据需要扩大多少倍
    uint8_t  DataType;      //原始数据的数据类型
    uint8_t  ReplyDataLen;  //645回复数据长度
    uint8_t  r_o_w;         //读写权限
    int32_t  w_min;         //写值下限
    int32_t  w_max;         //写值上限
    // uint8_t  data_src;      //数据来源，用于写后保存数据
    uint8_t  data_type;      //数据类型，用于对接逆变器控制
    uint8_t  DescribeType[16];//该类型的描述
}R_WDataTypeDef;

// 645只读数据结构体定义
typedef struct
{
    uint32_t DataIdf;       //645数据标识
    void    *DataBuff;     //回复数据地址
    float    DateRate;      //上送的数据需要扩大多少倍
    uint8_t  DataType;      //原始数据的数据类型
    uint8_t  ReplyDataLen;  //645回复数据长度
    uint8_t  DescribeType[16];//该类型的描述
}ReadDataTypeDef;

/* 645数据块结构体 */
typedef struct
{
    uint32_t DataIdf;      //645数据标识
    uint32_t ByteLen;       //数据块中的每个数据的字节长度
    uint32_t AllByteLen;    //总字节长度
    void    *ValArr[6];    //存储数据的地址,最大6个
    float    DateRate;      //上送的数据需要扩大多少倍
    uint8_t  DataType;      //原始数据的数据类型
    uint8_t  DescribeType[16];//该类型的描述
}ReadBlockDataTypeDef;


extern uint8_t sg_dl645_addr_bcd[];

uint8_t BCD2DEC(uint8_t ch);

void dlt645_ctrl_read_data(uint8_t fun_c, uint32_t id,  uint8_t uart_no);
void dlt645_ctrl_write_data(uint8_t fun_c, uint32_t id, uint8_t *p_buf, uint16_t len, uint8_t uart_no);



#endif
