#ifndef __UART_DEF_H__
#define __UART_DEF_H__

/* board.h里面定义了，这里只需要部分串口，所以就再定义一遍 */
#define USR_USING_UART1         /* 用于DLT645通信或者ModBus通信 */
//#define USR_USING_UART1_CTRL

//#define USR_USING_UART2       /* 用于... */
#define USR_USING_UART3         /* 用于串口无线 */
//#define USR_USING_UART4       /* 用于... */
//#define USR_USING_UART5       /* 用于... */
//#define USR_USING_UART6         /* 用于DLT645通信或者ModBus通信 */

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
#if defined(USR_USING_UART6)
    UART6_NO,
#endif
    UART_NO_MAXS,
};

enum
{
#if defined(USR_USING_UART1)
    RS485_UART_NO_1   = 1,
#endif
#if defined(USR_USING_UART2)
    RS485_UART_NO   = 2,
#endif
#if defined(USR_USING_UART3)
    WL_UART_NO   = 3,
#endif
#if defined(USR_USING_UART6)
    RS485_UART_NO_2   = 4,
#endif
    UART_NO_MAX     ,
};

#if defined(USR_USING_UART1)
#define UART1_NAME      "uart1"
#if defined(USR_USING_UART1_CTRL)
#define UART1_CTRL_PIN     GET_PIN(A, 8)
#endif
#endif

#if defined(USR_USING_UART3)
#define UART3_NAME      "uart3"
#if defined(USR_USING_UART3_CTRL)
#endif
#endif

#if defined(USR_USING_UART6)
#define UART6_NAME      "uart6"
#if defined(USR_USING_UART6_CTRL)
#endif
#endif





#endif
