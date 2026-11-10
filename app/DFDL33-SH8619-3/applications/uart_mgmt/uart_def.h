#ifndef __UART_DEF_H__
#define __UART_DEF_H__


enum CHECK_BIT
{
    UART_CHECK_8N1 = 1,
    UART_CHECK_8O1 = 2,
    UART_CHECK_8E1 = 3,
};

/* board.h里面定义了，这里只需要部分串口，所以就再定义一遍 */
#define USR_USING_UART1        // RJ45-1-2，第一组第二路
//#define USR_USING_UART1_CTRL

//#define USR_USING_UART2       /* 用于... */
#define USR_USING_UART3         // RS485-1
#define USR_USING_UART4         // RJ45-1-1，第一组第一路
#define USR_USING_UART5         // RJ45-2-2，第二组第二路
#define USR_USING_UART6         // RS485-Ⅱ
#define USR_USING_UART7         // RJ45-2-1，第二组第一路
#define USR_USING_UART8         // 载波

/* UARTx_NO是启用串口数组的下标，不等于硬件UART编号，也不等于档案端口号。 */
enum
{
#if defined(USR_USING_UART1)
    UART1_NO,   // 硬件UART1，RJ45-1-2
#endif
#if defined(USR_USING_UART2)
    RS485_UART_NO   = 2,
#endif
#if defined(USR_USING_UART3)
    UART3_NO,   // RS485-1
#endif
#if defined(USR_USING_UART4)
    UART4_NO,   // 硬件UART4，RJ45-1-1
#endif
#if defined(USR_USING_UART5)
    UART5_NO,   // 硬件UART5，RJ45-2-2
#endif
#if defined(USR_USING_UART6)
    UART6_NO,   // RS485-Ⅱ
#endif
#if defined(USR_USING_UART7)
    UART7_NO,   // 硬件UART7，RJ45-2-1
#endif
#if defined(USR_USING_UART8)
    UART8_NO,   // 载波
#endif
    UART_NO_MAXS,
};

#endif
