#include "dlt645_deal.h"
#include "dlt645_define.h"
#include <rtdevice.h>

#include "ctu_cfg.h"
#include "main_uart.h"
#include "dlt645_data_center.h"
#include "rng_buf.h"

#include "HJ02C.h"

#define DBG_TAG "dlt645"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define DLT645_ADDR_REAL_TIME_ENBALE    1   // 645地址实时生效宏定义

enum DLT645_Addr_Type
{
    DLT645_ADDR_UNICAST = 0,// 单播地址
    DLT645_ADDR_BROADCAST,  // 广播地址
    DLT645_ADDR_NOT_MATCH,  // 地址不匹配
};

struct DLT645_Pack_Str
{
    uint8_t Address[DL645_ADDR_SIZE]; // 地址
    uint8_t Addr_Type; // 地址类型
    union
    {
        uint8_t Control_Code;   // 控制码
        struct
        {
            uint8_t D0_D4               :5; // D4～D0功能码
            uint8_t D5                  :1; // 后续帧标志        0：无后续数据帧  1：有后续数据帧
            uint8_t D6                  :1; // 从站应答标志     0：从站正确应答  1：从站异常应答
            uint8_t D7                  :1; // 主从标志            0：主站发起的请求, 1：从站发起的请求
        };
    };
    uint32_t Ruler_ID;      // 规约ID
    uint8_t Data_Length;    // 数据长度
    uint8_t *Data;          // 数据
};
struct DLT645_Pack_Str DLT645_Pack = {0};

// 包校验
int32_t DLT645_Pack_Deal(uint8_t *bufPtr, uint16_t PackLen, uint8_t **newPackPtr, uint16_t *newPackLen)
{
    uint16_t usLenLeft = 0; // 循环检测之的长度
    uint8_t ucDataLen = 0;

    // 判断指针是否为空,判断长度是否小于最小帧字节数
    if((!bufPtr || !newPackPtr || !newPackLen) || (PackLen < D07_FRAME_LEN_MIN))
    {
        return -1;
    }

    for(uint16_t i = 0; i < PackLen; i++)
    {
        usLenLeft = PackLen - i;
        if(usLenLeft < D07_FRAME_LEN_MIN)
        {
            return -1;
        }
        // 查找0x68是否合理，判断帧尾是否合理
        if((bufPtr[i] == 0x68) && (0x68 == bufPtr[i+7]) && (0x16 == bufPtr[i+usLenLeft-1]))
        {
            uint8_t ucCheckSum = 0;
            for(uint16_t j = 0; j < usLenLeft-2; j++)
            {
                ucCheckSum += bufPtr[i+j];
            }
            // 判断CS是否正确
            if(ucCheckSum == bufPtr[i+usLenLeft-2])
            {
                ucDataLen = bufPtr[i+9];
                if (ucDataLen > D07_DATA_MAX_NR)
                {
                    return -1;
                }
                *newPackPtr = &bufPtr[i];
                *newPackLen = ucDataLen + 12; // 12 = 6地址 + 2起始符 + 1控制码 + 1数据长度 + 1校验和 + 1结束符
                return 0;
            }
        }
    }
    return -1;
}

/* 645地址验证 */
int32_t DLT645_Addr_Check(uint8_t *bufPtr, uint8_t *dlt645_addr)
{
    uint8_t match_cnt = 0;
    for(uint8_t i=0; i<DL645_ADDR_SIZE; i++)
    {
        if(bufPtr[i] == 0xAA)
        {
            match_cnt++;
        }
        if(bufPtr[i] == 0x99)
        {
            match_cnt++;
        }
    }
    if(match_cnt == DL645_ADDR_SIZE)
    {
        // 广播地址匹配
        DLT645_Pack.Addr_Type = DLT645_ADDR_BROADCAST;
        return 0;
    }

    // 检查地址是否匹配
    match_cnt = 0;
    for(uint8_t i=0; i<DL645_ADDR_SIZE; i++)
    {
        if(bufPtr[i] == dlt645_addr[i])
        {
            match_cnt++;
        }
    }
    if(match_cnt == DL645_ADDR_SIZE)
    {
        // 单播地址匹配
        DLT645_Pack.Addr_Type = DLT645_ADDR_UNICAST;
        return 0;
    }
    DLT645_Pack.Addr_Type = DLT645_ADDR_NOT_MATCH;
    return -1; // 地址不匹配

}

int dlt645_addr_ack(uint8_t uart_no)
{
    uint8_t g_packBuf[256] = {0};
    uint16_t packLen = 0;
    uint8_t  cs = 0;

    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    //1 生成应答报文
    g_packBuf[packLen++] = 0x68;

    for (uint8_t i=0; i<DL645_ADDR_SIZE; i++)
    {
        g_packBuf[packLen++] = sg_dl645_addr_bcd[i];
    }
    g_packBuf[packLen++] = 0x68;
    g_packBuf[packLen++] = 0x93;
    g_packBuf[packLen++] = 0x06;
    for (uint8_t i=0; i<DL645_ADDR_SIZE; i++)
    {
        g_packBuf[packLen++] = sg_dl645_addr_bcd[i] + 0x33;
    }
    for(uint16_t i = 4U; i < packLen; ++i)
    {
        cs += g_packBuf[i];
    }
    g_packBuf[packLen++] = cs;
    g_packBuf[packLen++] = 0x16;

    rt_kprintf("ctu addr ack : ");
    for(uint16_t i = 0U; i < packLen; ++i)
    {
        rt_kprintf("%02x", g_packBuf[i]);
    }
    rt_kprintf("\n");

//    if(HJ02C == uart_no)
//    {
//        hj02c_send(g_packBuf, packLen);
//    }else{
//        uart_mgmt_write(uart_no, g_packBuf, packLen);
//    }
    dlt645_data_ack(uart_no, g_packBuf, packLen);

    return 1;
}

/* 检查待写入通信地址的6个字节是否均为合法压缩BCD，并拒绝广播及通配地址。 */
static rt_bool_t dlt645_write_address_valid(const uint8_t address[DL645_ADDR_SIZE])
{
    uint8_t index; /* 当前检查的通信地址字节下标。 */
    rt_bool_t all_aa = RT_TRUE; /* AA AA AA AA AA AA是广播读写地址，不能保存为设备自身地址。 */
    rt_bool_t all_99 = RT_TRUE; /* 99 99 99 99 99 99是通配地址，不能保存为设备自身地址。 */

    for(index = 0U; index < DL645_ADDR_SIZE; ++index)
    {
        if(((address[index] & 0x0FU) > 9U) || (((address[index] >> 4U) & 0x0FU) > 9U)) /* 每个半字节必须是0～9的十进制BCD。 */
        {
            return RT_FALSE;
        }
        if(address[index] != 0xAAU) /* 任一字节不是AA即可排除全广播地址。 */
        {
            all_aa = RT_FALSE;
        }
        if(address[index] != 0x99U) /* 任一字节不是99即可排除全通配地址。 */
        {
            all_99 = RT_FALSE;
        }
    }
    return ((all_aa == RT_FALSE) && (all_99 == RT_FALSE));
}

/* 发送写通信地址正常或异常应答，正常应答使用已经生效的新通信地址。 */
static rt_err_t dlt645_write_address_ack(uint8_t uart_no, uint8_t err_code)
{
    uint8_t response[32] = {0}; /* 写地址应答不携带业务数据，32字节局部缓冲足够容纳前导码和异常字。 */
    uint16_t length = 0U; /* 当前已经写入应答缓冲的字节数量。 */
    uint8_t checksum = 0U; /* 从第一个68开始累加得到的8位校验和。 */
    uint16_t index; /* 地址、校验和及发送长度循环使用的下标。 */

    response[length++] = 0xFEU; /* 保持工程现有645应答格式，发送4个前导字节。 */
    response[length++] = 0xFEU;
    response[length++] = 0xFEU;
    response[length++] = 0xFEU;
    response[length++] = 0x68U;
    for(index = 0U; index < DL645_ADDR_SIZE; ++index)
    {
        response[length++] = sg_dl645_addr_bcd[index]; /* 成功时运行地址已切换，因此此处自然使用新地址。 */
    }
    response[length++] = 0x68U;
    if(err_code == E_D07_W_OK) /* 正常写地址应答控制码为0x95且数据域长度为0。 */
    {
        response[length++] = E_D07_CTRL_WRITE_ADDR | 0x80U;
        response[length++] = 0U;
    }
    else /* 地址长度或BCD内容非法时返回0xD5异常应答及一字节错误状态。 */
    {
        response[length++] = E_D07_CTRL_WRITE_ADDR | 0xC0U;
        response[length++] = 1U;
        response[length++] = err_code + 0x33U;
    }
    for(index = 4U; index < length; ++index)
    {
        checksum += response[index];
    }
    response[length++] = checksum;
    response[length++] = 0x16U;
    show_arr("dlt645 write address ack :", response, length); /* 输出完整应答便于确认新地址及控制码。 */
    return dlt645_data_ack(uart_no, response, length);
}

/* 处理0x15写通信地址命令，校验通过后同步更新Flash配置和当前运行地址。 */
static void dlt645_write_address(uint8_t uart_no, const uint8_t *address, uint8_t data_length)
{
    if((data_length != DL645_ADDR_SIZE) || (dlt645_write_address_valid(address) == RT_FALSE)) /* 数据域必须恰好为6字节合法BCD地址。 */
    {
        LOG_E("dlt645 write address invalid, length=%d", data_length);
        dlt645_write_address_ack(uart_no, E_D07_W_ERR); /* 地址未变更，异常应答继续使用当前地址。 */
        return;
    }

    rt_memcpy(ctu_cfg.dlt645_bcd_addr, address, DL645_ADDR_SIZE); /* 先更新配置对象，后续保存内容与运行地址保持一致。 */
    ctu_cfg_save(); /* 将新通信地址保存到配置A/B区，使重新上电后仍然有效。 */
    rt_memcpy(sg_dl645_addr_bcd, address, DL645_ADDR_SIZE); /* 保存完成后立即切换当前645从站匹配及应答地址。 */
    LOG_I("dlt645 address changed to %02x%02x%02x%02x%02x%02x",
          address[5], address[4], address[3], address[2], address[1], address[0]); /* 日志按高位到低位显示12位地址。 */
    dlt645_write_address_ack(uart_no, E_D07_W_OK); /* 使用新地址返回0x95正常应答。 */
}

/* 645协议解析 */
void dlt645_deal(uint8_t uart_no, uint8_t *dlt645_addr, uint8_t *bufPtr, uint16_t PackLen)
{
    uint8_t *newPackPtr = NULL;
    uint16_t newPackLen = 0;

    // 判断帧是否合理
    if(-1 == DLT645_Pack_Deal(bufPtr, PackLen, &newPackPtr, &newPackLen))
    {
        show_arr("DLT645 Check Err", bufPtr, PackLen);
        return;
    }
    else
    {
        show_arr("DLT645 Check Success", newPackPtr, newPackLen);
    }

    // 解析报文
    rt_memcpy(DLT645_Pack.Address, &newPackPtr[1], DL645_ADDR_SIZE);
    if(-1 == DLT645_Addr_Check(DLT645_Pack.Address, dlt645_addr))
    {
        show_arr("DLT645 Addr Not Match", DLT645_Pack.Address, DL645_ADDR_SIZE);
        return;
    }

    DLT645_Pack.Control_Code = newPackPtr[8];   // 控制码
    DLT645_Pack.Data_Length = newPackPtr[9];    // 长度
    DLT645_Pack.Data = &newPackPtr[10];         // 数据
    for(uint16_t i = 0U; i < DLT645_Pack.Data_Length; ++i)
    {
        DLT645_Pack.Data[i] -= 0x33;
    }


    if((DLT645_Pack.Addr_Type == DLT645_ADDR_UNICAST) || (DLT645_Pack.Addr_Type == DLT645_ADDR_BROADCAST))
    {
        if(E_D07_CTRL_DIR_M2S == DLT645_Pack.D7)
        {
            switch(DLT645_Pack.D0_D4)
            {
                case E_D07_CTRL_READ_DATA:
                    // 处理读数据
                    rt_memcpy(&DLT645_Pack.Ruler_ID, &newPackPtr[10], 4);
                    dlt645_ctrl_read_data(E_D07_CTRL_READ_DATA, DLT645_Pack.Ruler_ID, uart_no);
                    break;
                case E_D07_CTRL_WRITE_DATA:
                    // 处理写数据
                    rt_memcpy(&DLT645_Pack.Ruler_ID, &newPackPtr[10], 4);
                    DLT645_Pack.Data_Length = newPackPtr[9] - 4;
                    DLT645_Pack.Data = &newPackPtr[14];
                    dlt645_ctrl_write_data(E_D07_CTRL_WRITE_DATA, DLT645_Pack.Ruler_ID, DLT645_Pack.Data, DLT645_Pack.Data_Length, uart_no);
                    break;
                case E_D07_CTRL_PRIVATE_GSE:
//                    rt_kprintf("Receive Private645\n");
                    rt_memcpy(&DLT645_Pack.Ruler_ID, &newPackPtr[10], 4);
                    DLT645_Pack.Data_Length = newPackPtr[9] - 4;
                    DLT645_Pack.Data = &newPackPtr[14];
                    dlt645_upgrade_manage(DLT645_Pack.Ruler_ID, DLT645_Pack.Data, DLT645_Pack.Data_Length);
                    break;
                case E_D07_CTRL_READ_ADDR:
                    rt_kprintf("recv dl645 read addr\n");
                    dlt645_addr_ack(uart_no);
                    break;
                case E_D07_CTRL_WRITE_ADDR:
                    rt_kprintf("recv dl645 write addr\n");
                    dlt645_write_address(uart_no, DLT645_Pack.Data, DLT645_Pack.Data_Length); /* 数据域已在统一解析阶段完成减0x33。 */
                    break;
                case E_D07_CTRL_SYNC_TIME:
                    rt_kprintf("recv dl645 broadcast time adjist\n");
                    uint16_t year = BCD2DEC(DLT645_Pack.Data[5]) + 2000;
                    uint16_t mon  = BCD2DEC(DLT645_Pack.Data[4]);
                    uint16_t mday = BCD2DEC(DLT645_Pack.Data[3]);
                    uint16_t hour = BCD2DEC(DLT645_Pack.Data[2]);
                    uint16_t min  = BCD2DEC(DLT645_Pack.Data[1]);
                    uint16_t sec  = BCD2DEC(DLT645_Pack.Data[0]);
                    set_date(year, mon, mday);
                    set_time(hour, min, sec);
                    break;
                default:
                    // 其他控制码处理
                    show_arr("recv other dl645", (uint8_t*)newPackPtr, newPackLen);
                    break;
            }
        }
    }
}

struct rt_semaphore sem_dlt645;             //645处理线程信号量

struct rng_buf dlt645_rng;

uint8_t dlt645_rx_buf[1024];

void Dlt645_Init(void)
{
    if(rt_sem_init(&sem_dlt645, "dlt645", 0, RT_IPC_FLAG_FIFO) != RT_EOK)
    {
        rt_kprintf("creat sem_dlt645 failed!\n");
    }

    RngBufInit(&dlt645_rng,   dlt645_rx_buf,   sizeof(dlt645_rx_buf), RNG_BUF_MODE_SINGLE);
}

void dlt645_rx_callback(void *ptr, uint16_t len, uint16_t buf_source)
{
    struct rngbuf_queue queue = {buf_source, len};
    RngBufWrite(&dlt645_rng, &queue, sizeof(queue));
    RngBufWrite(&dlt645_rng, ptr, len);

    //释放信号量，线程中去获取报文
    rt_sem_release(&sem_dlt645);
}

void dlt645_rx_get(void *ptr, uint16_t *len, uint16_t *buf_source)
{
    struct rngbuf_queue queue = {0};
    RngBufRead(&dlt645_rng, &queue, sizeof(queue));
    *buf_source = queue.buf_source;
    *len = queue.len;

    RngBufRead(&dlt645_rng, ptr, queue.len);
}

rt_err_t dlt645_data_ack(uint16_t uart_no, const void *buffer, rt_size_t size)
{
    if(HJ02C == uart_no)
    {
        return hj02c_send(buffer, size);
    }
    else{
        return (uart_mgmt_write(uart_no, buffer, size) == size) ? RT_EOK : -RT_ERROR;
    }
}

#define DLT645_DEAL_RX_BUFFER_SIZE 1024U /* 协议库写请求含4个前导字节时整帧为266字节，处理缓冲必须完整容纳。 */
uint8_t dlt645_deal_rx_buf[DLT645_DEAL_RX_BUFFER_SIZE] = {0};
uint16_t dlt645_deal_rx_len = 0;

/* 645协议处理线程 */
void dlt645_deal_thread_entry(void* parameter)
{
    for(uint8_t i=0; i<DL645_ADDR_SIZE; i++)
    {
        sg_dl645_addr_bcd[i] = ctu_cfg.dlt645_bcd_addr[i];
    }

    while(1)
    {
        rt_sem_take(&sem_dlt645, RT_WAITING_FOREVER);
#if DLT645_ADDR_REAL_TIME_ENBALE
        for(uint8_t i=0; i<DL645_ADDR_SIZE; i++)
        {
            sg_dl645_addr_bcd[i] = ctu_cfg.dlt645_bcd_addr[i];/*地址实时生效*/
        }
#endif

        uint16_t uart_type = 0;
        //获取需要处理的串口号信息及该串口接收数据
        dlt645_rx_get(dlt645_deal_rx_buf, &dlt645_deal_rx_len, &uart_type);

        //数据解析
        dlt645_deal(uart_type, sg_dl645_addr_bcd, dlt645_deal_rx_buf, dlt645_deal_rx_len);
    }
}


//void test_dlt645_ack(int argc, void** argv)
//{
//    char test_pack_buff[60] = {0};
//    rt_kprintf("%s\n", argv[1]);
//    char *p = argv[1];
//
//    int i=0;
//    while(*(p+i*2) != '\0')
//    {
//        sscanf(p+i*2, "%2x", (unsigned int *)&test_pack_buff[i]);
//        i++;
//    }
//
//    struct rng_buf* write_rng_buf = RT_NULL;
//    RngBufWrite(write_rng_buf, test_pack_buff, i);
//}
//MSH_CMD_EXPORT(test_dlt645_ack, test_dlt645_ack);

