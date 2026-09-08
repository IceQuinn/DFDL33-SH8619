#ifndef __UPGRADE_H__
#define __UPGRADE_H__

#include <stdint.h>
#include <rtthread.h>

#pragma pack(1) //一字节对齐
struct IAP_Hand_Pack
{
    uint8_t  Upgrade_Type;      // 升级类型，1:Boot，2:App
    uint16_t Upgrade_Ver;       // 升级版本
    uint16_t File_Divide_Size;  // 分片大小（每包数据数）
    uint16_t File_All_Divids;   // 总分片数（总包数）
    uint32_t File_Size;         // 程序总大小
    uint32_t File_CRC;          // 程序CRC32
};

struct IAP_Master_Updata_Pack
{
    uint16_t File_Divide_idx;   // 分包序号,1~n
    uint16_t File_Divide_Len;   // 分包长度
    uint8_t  File_Divide_Data[240]; // 升级包
};
#pragma pack()

/* 升级消息类型 */
typedef enum {
    UPGRD_MSG_HAND        = 0x01,  // 摘要帧
    UPGRD_MSG_UPDATA      = 0x02,  // 升级数据帧
    UPGRD_MSG_RETRANS     = 0x03,  // 重传帧
    UPGRD_MSG_CHECK       = 0x04,  // 校验帧
} upgrd_msg_type_t;


/* 消息结构体 */
typedef struct {
    upgrd_msg_type_t type;         // 消息类型
    uint16_t         len;          // 有效数据长度
    uint8_t          data[240];    // 数据载荷
} upgrd_msg_t;

extern rt_mq_t upgrd_mq;




#endif
