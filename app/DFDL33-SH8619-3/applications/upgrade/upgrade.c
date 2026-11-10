#include "upgrade.h"

#include <rtthread.h>
#include <string.h>
#include "sys.h"
#include "drv_ex_flash.h"
#include "crc32.h"
#include "HJ02C.h"
#include "crc16.h"
#include "user_iwdg.h"

#define RETRANSMISSION_DEBUG    0
#define CRC_DEBUG               0

#define EXTFLASH_PAGE_SIZE      (4096)
#define HAND_PACK_ADDDR         (1049 * EXTFLASH_PAGE_SIZE)         //摘要数据写入外部flash的起始地址
#define START_ADDR              (1050 * EXTFLASH_PAGE_SIZE)         //APP写入外部flash的起始地址
#define WRITE_ADDR              (1042 * EXTFLASH_PAGE_SIZE)         //BOOT+APP写入外部flash的起始地址
#define MY_SET_BIT(x, n)        ((x) |= (1 << ((n - 1) % 8)))       //升级数据错误置位
#define FLASH_ERASE             (496 * FLASH_PAGE_SIZE)             //擦除扇区大小


uint8_t BitMap_Data[200];
uint32_t Get_Updata_Pack_Size = 0;                                  //记录获取升级包字节数
struct IAP_Hand_Pack  IAP_Hand_Pack;                                //摘要帧
extern uint8_t sg_dl645_addr_bcd[];

uint8_t is_bitmap_complete(void)
{
    uint16_t total_pack = IAP_Hand_Pack.File_All_Divids;
    uint16_t byte_num   = total_pack / 8;
    uint8_t  bit_remain = total_pack % 8;

    for (uint16_t i = 0; i < byte_num; i++) {
        if (BitMap_Data[i] != 0xFF) {
            return 0;//未收齐
        }
    }

    if (bit_remain != 0) {
        uint8_t mask = (1 << bit_remain) - 1;
        if ((BitMap_Data[byte_num] & mask) != mask) {
            return 0;//未收齐
        }
    }

    return 1;  //收齐
}

int data_ack(const void* databuf, uint16_t datalen)
{
    if(datalen > 240)
        return -1;
    const uint8_t *p = (const uint8_t *)databuf;
    uint8_t g_packBuf[256] = {0};
    uint16_t packLen = 0;
    uint8_t  cs = 0;

    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    g_packBuf[packLen++] = 0xFE;
    //1 生成应答报文
    g_packBuf[packLen++] = 0x68;

    for (uint8_t i=0; i<6; i++)
    {
        g_packBuf[packLen++] = sg_dl645_addr_bcd[i];
    }
    g_packBuf[packLen++] = 0x68;
    g_packBuf[packLen++] = 0x9F;
    g_packBuf[packLen++] = datalen;
    for(uint16_t i = 0U; i < datalen; ++i)
    {
        g_packBuf[packLen++] = p[i] + 0x33;
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

    hj02c_send(g_packBuf, packLen);

    return 1;
}

void Save_Hand_Pack(void)
{
    struct Save_Hand_Str save_hand;
    save_hand.Upgrade_Flag = 1;
    save_hand.Upgrade_Type = IAP_Hand_Pack.Upgrade_Type;
    save_hand.Upgrade_Ver  = IAP_Hand_Pack.Upgrade_Ver;
    save_hand.File_Size    = IAP_Hand_Pack.File_Size;
    save_hand.File_CRC     = IAP_Hand_Pack.File_CRC;
    save_hand.Hand_CRC16   = crc16(&save_hand, sizeof(struct Save_Hand_Str)-2);

    if(flash_write(HAND_PACK_ADDDR, &save_hand, sizeof(struct Save_Hand_Str)) != RT_EOK)
    {
        rt_kprintf("Save Hand Failed!!!\n");
    }
}


/****************************摘要帧处理******************************/

int Hand_Pack_Deal(uint32_t UART_Rx_Len, uint8_t UART_Rx_Buf[])
{
    struct IAP_Hand_Pack *Ptr_IAP_Hand_Pack = (struct IAP_Hand_Pack *)UART_Rx_Buf;
    uint8_t ack_data[256];
    uint32_t di = 0x08800001;

    rt_kprintf("HEAD_HANDS RngBufDataSize = %d\r\n", UART_Rx_Len);

    rt_kprintf("[%08d]Upgrade_Type = %d, Upgrade_Ver = %d, File_Divide_Size = %d, File_All_Divids = %d, File_Size = %d, File_CRC = %x\r\n",
                rt_tick_get(),
                Ptr_IAP_Hand_Pack->Upgrade_Type,
                Ptr_IAP_Hand_Pack->Upgrade_Ver,
                Ptr_IAP_Hand_Pack->File_Divide_Size,
                Ptr_IAP_Hand_Pack->File_All_Divids,
                Ptr_IAP_Hand_Pack->File_Size,
                Ptr_IAP_Hand_Pack->File_CRC);

    memcpy(ack_data, &di, sizeof(di));
    memcpy(ack_data + 4, Ptr_IAP_Hand_Pack, sizeof(struct IAP_Hand_Pack));
    //回复摘要
    data_ack(ack_data, sizeof(struct IAP_Hand_Pack) + 4);

    show_arr("IAP_Hand_Pack Send", Ptr_IAP_Hand_Pack, sizeof(struct IAP_Hand_Pack));
    memcpy(&IAP_Hand_Pack, Ptr_IAP_Hand_Pack, sizeof(struct IAP_Hand_Pack));
    return 0;
}


/*****************************升级帧处理*****************************/

uint16_t updata_len;                    //用来记录升级包长度
uint16_t rx_successful_count;           //用来记录成功接收数据包的个数

#if RETRANSMISSION_DEBUG
//用于重传测试
void BitMap_test(void)
{
    BitMap[0]  = 0xfe;
    BitMap[10] = 0x1e;
    BitMap[19] = 0x7e;
    BitMap[20] = 0x0;
    BitMap[21] = 0x0;
    BitMap[30] = 0x7e;
    BitMap[51] = 0x0;
    BitMap[52] = 0x7e;
    BitMap[61] = 0x0;
    BitMap[62] = 0x7e;
}
#endif

uint32_t start_addr;
uint32_t write_flash_size;

#if CRC_DEBUG
uint32_t crc32_cal_r = 0;
uint32_t crc32_cal_w = 0;
uint8_t  updata_buf_r[4096];
#endif

int Updata_Pack_Deal(uint32_t UART_Rx_Len, uint8_t UART_Rx_Buf[])
{
    rt_kprintf("Updata_Pack start: %08d\n", rt_tick_get());
    uint8_t  updata_buf[4096];              //用于临时存放升级包数据，为写入外部flash做准备
    uint16_t updata_len = IAP_Hand_Pack.File_Divide_Size;
    rt_kprintf("DATA_HANDS RngBufDataSize = %d\r\n", UART_Rx_Len);
    struct IAP_Master_Updata_Pack *Ptr_IAP_Master_Updata_Pack = (struct IAP_Master_Updata_Pack *)UART_Rx_Buf;


    if((1 == Ptr_IAP_Master_Updata_Pack->File_Divide_idx))
    {
        uint32_t value; // 用于存放合并后的32位值
        // 从File_Divide_Data[4]开始，拷贝4个字节到value中
        memcpy(&value, &Ptr_IAP_Master_Updata_Pack->File_Divide_Data[4], sizeof(value));
        rt_kprintf("value = %x\r\n", value);
        if(value > 0x8008000)
        {
            rt_kprintf("Address offset is greater than 0x8008000\r\n");
            start_addr = START_ADDR;
            write_flash_size = IAP_Hand_Pack.File_Size;
        }
        else if(value < 0x8008000)
        {
            rt_kprintf("Address offset is less than 0x8008000!!!\r\n");
//            start_addr = WRITE_ADDR;
//            write_flash_size = IAP_Hand_Pack.File_Size - (8 * EXTFLASH_PAGE_SIZE);
            start_addr = START_ADDR;
            write_flash_size = IAP_Hand_Pack.File_Size;
        }
    }

    Get_Updata_Pack_Size += Ptr_IAP_Master_Updata_Pack->File_Divide_Len;
    rx_successful_count++;
    rt_kprintf("[%08d]IAP UpDate Get, File_Divide_idx = %03d/%d, File_Divide_Len = %d, Rx_Successful_Count = %d/%d, Get_Updata_Pack_Size = %d/%d\r\n",
            rt_tick_get(),
            Ptr_IAP_Master_Updata_Pack->File_Divide_idx,
            IAP_Hand_Pack.File_All_Divids,
            Ptr_IAP_Master_Updata_Pack->File_Divide_Len,
            rx_successful_count,
            IAP_Hand_Pack.File_All_Divids,
            Get_Updata_Pack_Size,
            IAP_Hand_Pack.File_Size);


    //置位
    MY_SET_BIT(BitMap_Data[(Ptr_IAP_Master_Updata_Pack->File_Divide_idx - 1) / 8], Ptr_IAP_Master_Updata_Pack->File_Divide_idx);

    //计算出该帧对应的扇区；
    uint32_t addr = ((start_addr + updata_len * (Ptr_IAP_Master_Updata_Pack->File_Divide_idx - 1)) / 4096) * 4096;

    //读出该扇区中的数据;
    flash_read(addr, updata_buf, sizeof(updata_buf));

    //对应扇区（缓冲区）偏移
    uint32_t offset_in_sector = (updata_len * (Ptr_IAP_Master_Updata_Pack->File_Divide_idx - 1)) % 4096;

    //跨扇数据处理
    if(offset_in_sector + Ptr_IAP_Master_Updata_Pack->File_Divide_Len > EXTFLASH_PAGE_SIZE)
    {
        uint32_t bytes_in_current = EXTFLASH_PAGE_SIZE - offset_in_sector;
        uint32_t bytes_in_next = Ptr_IAP_Master_Updata_Pack->File_Divide_Len - bytes_in_current;

        //将数据插入
        memcpy(&updata_buf[offset_in_sector], Ptr_IAP_Master_Updata_Pack->File_Divide_Data, bytes_in_current);

        // 刷入当前逻辑扇区
        flash_write(addr, updata_buf, sizeof(updata_buf));
        memset(updata_buf, 0, sizeof(updata_buf));
        offset_in_sector = 0;

        // 切换到新扇区
        addr += 4096;
        flash_read(addr, updata_buf, sizeof(updata_buf));
        //将数据插入
        memcpy(&updata_buf[offset_in_sector], Ptr_IAP_Master_Updata_Pack->File_Divide_Data + bytes_in_current, bytes_in_next);
        flash_write(addr, updata_buf, sizeof(updata_buf));
        memset(updata_buf, 0, sizeof(updata_buf));
    }
    else {
        //将数据插入
        memcpy(&updata_buf[offset_in_sector], Ptr_IAP_Master_Updata_Pack->File_Divide_Data, Ptr_IAP_Master_Updata_Pack->File_Divide_Len);
        flash_write(addr, updata_buf, sizeof(updata_buf));
        memset(updata_buf, 0, sizeof(updata_buf));
    }

#if RETRANSMISSION_DEBUG
//                BitMap_test();
#endif
#if CRC_DEBUG
//                crc32_cal_w = crc32_cal_w ^ 0xffffffff;
//                crc32_cal_r = crc32_cal_r ^ 0xffffffff;
//                MyPrintf("[%08d]crc32_cal_w = %x, crc32_cal_r = %x\r\n", AT32_GetTick(), crc32_cal_w,crc32_cal_r);
#endif

    rt_kprintf("Updata_Pack end: %08d\n", rt_tick_get());
    return 0;
}


//void test_flash_rw(void)
//{
//    uint8_t test_buf1[6] = {1, 2, 3, 4, 5, 6};
//    uint8_t test_buf2[10];
//    uint8_t test_buf3[6] = {9, 10, 11, 12, 13, 14};
//    flash_read(200*4096, test_buf2, sizeof(test_buf2));
//
//    flash_write(200*4096, test_buf1, sizeof(test_buf1));
//
//    flash_read(200*4096, test_buf2, sizeof(test_buf2));
//
//    flash_write((200*4096+4), test_buf3, sizeof(test_buf3));
//
//    flash_read(200*4096, test_buf2, sizeof(test_buf2));
//}
//
//MSH_CMD_EXPORT(test_flash_rw, test_flash_rw);

//
//
/*****************************校验帧处理*****************************/

int Check_Pack_Deal(void)
{
    uint8_t  Check_Result = 0;
    uint32_t crc32_r_cal  = 0;
    uint32_t bytes_read   = 0;
//    uint32_t readaddr     = start_addr;
    uint32_t readaddr     = START_ADDR;
    uint8_t  updata_buf[4096];

    uint8_t ack_data[256];
    uint32_t di = 0x08800004;

    rt_kprintf("Check_Pack start: %08d\n", rt_tick_get());

    while (bytes_read < IAP_Hand_Pack.File_Size)
    {
        //未读取剩余数量
        uint32_t remaining = IAP_Hand_Pack.File_Size - bytes_read;
        uint16_t read_size = (remaining > EXTFLASH_PAGE_SIZE) ? EXTFLASH_PAGE_SIZE : remaining;
        flash_read(readaddr, updata_buf, read_size);

        crc32_r_cal = crc32_part(updata_buf, read_size, &crc32_r_cal);

        readaddr   += read_size;
            //已读取数量
        bytes_read += read_size;
        rt_thread_mdelay(10);
    }

    //读取完毕
    if(bytes_read == IAP_Hand_Pack.File_Size)
    {
        crc32_r_cal = crc32_r_cal ^ 0xffffffff;
        rt_kprintf("[%08d]full_crc32_r_cal = %x\r\n", rt_tick_get(), crc32_r_cal);
    }

    //校验结果判断
    if(crc32_r_cal == IAP_Hand_Pack.File_CRC)
    {
        Check_Result = 0x01;
        rt_kprintf("Check Pack Success\n");
        //升级标志写入外部flash
    }
    else
    {
        Check_Result = 0x02;
        rt_kprintf("Check Pack Failed\n");
    }

    //回复校验结果
    memcpy(ack_data, &di, sizeof(di));
    ack_data[4] = Check_Result;
    data_ack(ack_data, 5);
    rt_kprintf("Check_Pack end: %08d\n", rt_tick_get());
    if(0x01 == Check_Result)
    {
        //保存摘要
        Save_Hand_Pack();
        //重启
        reboot();
    }

    return 0;
}


/*****************************重传帧处理*****************************/

int Retransmission_Pack_Deal(uint32_t UART_Rx_Len,uint8_t UART_Rx_Buf[])
{
    uint8_t ack_data[256];
    uint32_t di = 0x08800003;

    //回复BitMap
    memcpy(ack_data, &di, sizeof(di));
    memcpy(ack_data + 4, BitMap_Data, sizeof(BitMap_Data));
    data_ack(ack_data, sizeof(BitMap_Data) + 4);

    rt_kprintf("[%08d]Successfully Sent The Bitmap\r\n", rt_tick_get());
    if(1 == is_bitmap_complete())
    {
//            rt_thread_mdelay(200);
        Check_Pack_Deal();
    }
    return 0;
}

rt_mq_t upgrade_mq;

#define UPGRD_MQ_LEN    10   // 队列可缓存的消息数

void upgrade_mq_init(void)
{
    upgrade_mq = rt_mq_create("upgrd_mq",
                            sizeof(upgrd_msg_t),  // 每条消息的大小
                            UPGRD_MQ_LEN,
                            RT_IPC_FLAG_FIFO);
    RT_ASSERT(upgrade_mq != RT_NULL);
}

int check_flg = 0;
void upgrade_thread_entry(void *param)
{
    upgrd_msg_t msg;
    rt_err_t ret;

    upgrade_mq_init();

    while (1)
    {
        ret = rt_mq_recv(upgrade_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        if (ret != RT_EOK) {
            continue;
        }

        /* 根据消息类型调用对应的处理函数 */
        switch (msg.type) {
        case UPGRD_MSG_HAND:
            Hand_Pack_Deal(msg.len, msg.data);
            break;

        case UPGRD_MSG_UPDATA:
            Updata_Pack_Deal(msg.len, msg.data);
            break;

        case UPGRD_MSG_RETRANS:
            Retransmission_Pack_Deal(msg.len, msg.data);
            break;

        case UPGRD_MSG_CHECK:
            Check_Pack_Deal();
            break;

        default:
            rt_kprintf("Unknown upgrade msg type: %d\r\n", msg.type);
            break;
        }
    }
}




