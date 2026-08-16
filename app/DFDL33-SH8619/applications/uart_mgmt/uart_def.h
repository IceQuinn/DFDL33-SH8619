#ifndef __UART_DEF_H__
#define __UART_DEF_H__


enum CHECK_BIT
{
    UART_CHECK_8N1 = 1,
    UART_CHECK_8O1 = 2,
    UART_CHECK_8E1 = 3,
};

/* board.h里面定义了，这里只需要部分串口，所以就再定义一遍 */
#define USR_USING_UART1        // RS485-2
//#define USR_USING_UART1_CTRL

//#define USR_USING_UART2       /* 用于... */
#define USR_USING_UART3         // RJ45-1-1
#define USR_USING_UART4         // RJ45-1-2
#define USR_USING_UART5         // RJ45-2-1
#define USR_USING_UART6         // RJ45-2-2
#define USR_USING_UART7         // RS485-1
#define USR_USING_UART8         // 载波

enum
{
#if defined(USR_USING_UART1)
    UART1_NO,
#endif
#if defined(USR_USING_UART2)
    RS485_UART_NO   = 2,
#endif
#if defined(USR_USING_UART3)
    UART3_NO,
#endif
#if defined(USR_USING_UART4)
    UART4_NO,
#endif
#if defined(USR_USING_UART5)
    UART5_NO,
#endif
#if defined(USR_USING_UART6)
    UART6_NO,
#endif
#if defined(USR_USING_UART7)
    UART7_NO,
#endif
#if defined(USR_USING_UART8)
    UART8_NO,
#endif
    UART_NO_MAXS,
};

#endif
