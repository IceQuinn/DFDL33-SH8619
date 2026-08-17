#ifndef __DLT645_DEFINE_H__
#define __DLT645_DEFINE_H__

#define DL645_ADDR_SIZE     6

#define D07_DATA_MAX_NR      256    /* dlt645 2007 5.2.4数据域长度，读数据最大200，写数据最大50 */
#define D07_FRAME_LEN_MIN    12     /* DLT645 2007 最小帧字节数 */

/* 控制码域 C 帧传送方向 */
typedef enum
{
    E_D07_CTRL_DIR_M2S, // 主站到从站
    E_D07_CTRL_DIR_S2M, // 从站到主站

}E_D07_CTRL_DIR;

/* 从站异常标志 */
typedef enum
{
    E_D07_CTRL_SR_OK, // 从站正常应答
    E_D07_CTRL_SR_NO, // 从站异常应答

}E_D07_CTRL_SR;

/* 控制域C 功能码 */
typedef enum
{
    E_D07_CTRL_RESV                 = 0x00,// 保留
    E_D07_CTRL_SYNC_TIME            = 0x08,// 广播校时
    E_D07_CTRL_READ_DATA            = 0x11,// 读数据
    E_D07_CTRL_READ_AFTERDATA       = 0x12,// 读后续数据
    E_D07_CTRL_READ_ADDR            = 0x13,// 读通信地址
    E_D07_CTRL_WRITE_DATA           = 0x14,// 写数据
    E_D07_CTRL_WRITE_ADDR           = 0x15,// 写通信地址
    E_D07_CTRL_FREEZ_COMM           = 0x16,// 冻结命令
    E_D07_CTRL_MODIFY_BAUD          = 0x17,// 修改通信速率
    E_D07_CTRL_MODIFY_PASSWORD      = 0x18,// 修改密码
    E_D07_CTRL_CLEAR_MAXDEMAND      = 0x19,// 最大需量清零
    E_D07_CTRL_CLEAR_METER          = 0x1A,// 电表清零
    E_D07_CTRL_CLEAR_EVENT          = 0x1B,// 事件清零
    E_D07_CTRL_COMM                 = 0x1C, // 控制命令
    E_D07_CTRL_PRIVATE_GSE          = 0x1F,// GSE私有协议和点表私有协议

}E_D07_CTRL_FNC;

#endif
