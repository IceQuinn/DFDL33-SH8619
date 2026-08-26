#include "dlt645_deal.h"
#include "dlt645_define.h"
#include <rtthread.h>
#include <rtdevice.h>

#include "ctu_cfg.h"
#include "main_uart.h"
#include "dlt645_data_center.h"
#include "rng_buf.h"

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
    for (uint8_t i=4; i<packLen; i++)
    {
        cs += g_packBuf[i];
    }
    g_packBuf[packLen++] = cs;
    g_packBuf[packLen++] = 0x16;

    rt_kprintf("ctu addr ack : ");
    for(uint8_t i=0; i<packLen; i++)
    {
        rt_kprintf("%02x", g_packBuf[i]);
    }
    rt_kprintf("\n");

    uart_mgmt_write(uart_no, g_packBuf, packLen);

    return 1;
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
    for(uint8_t i=0; i< DLT645_Pack.Data_Length; ++i)
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
                    rt_kprintf("Receive Private645\n");
                    // DLT645_Pack.Data_Length = newPackPtr[9];
                    // DLT645_Pack.Data = &newPackPtr[10];

                    break;
                case E_D07_CTRL_READ_ADDR:
                    rt_kprintf("recv dl645 read addr\n");
                    dlt645_addr_ack(uart_no);
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

uint8_t dlt645_deal_rx_buf[256] = {0};
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

