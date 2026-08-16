/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-10     mutou       the first version
 */
#include "main_uart.h"

#include <rtdevice.h>
#include <drv_common.h>

#include "ctu_cfg.h"
#include "cycle_loop.h"

// 收到最后一个字节后，超过该时间仍没有新数据，串口管理线程就认为当前数据已经组成一帧
// 实际时间精度受 RT_TICK_PER_SECOND 限制，需要修改帧间隔时只修改 UART_FRAME_TIMEOUT_MS
#define UART_RX_BUF_SIZE          256
#define UART_FRAME_TIMEOUT_MS     10
#define UART_RX_POLL_MS           1

typedef struct
{
    uint8_t uart_data_bits;
    uint8_t uart_stop_bits;
    uint8_t uart_parity;
}uart_check;

// 单个串口的接收管理结构体，rx_buf 由接收回调写入，frame_buf 由管理线程处理
// 使用双缓冲可避免上层处理当前帧时覆盖下一帧的接收数据
typedef struct
{
    uint8_t rx_buf[UART_RX_BUF_SIZE];       // 当前正在接收的数据
    uint8_t frame_buf[UART_RX_BUF_SIZE];    // 已经截取出的完整帧
    volatile rt_size_t len;                 // rx_buf 中已有的字节数
    rt_size_t frame_len;                    // frame_buf 中的帧长度
    volatile rt_tick_t last_tick;           // 最后一次收到字节时的系统节拍
    rt_tick_t timeout_tick;                 // 帧间超时对应的系统节拍数
    volatile rt_bool_t overflow;            // 当前帧是否超过接收缓冲区
    rt_bool_t frame_overflow;               // 截取出的帧是否发生过溢出
}uart_rx_manage;

// 串口设备
rt_device_t uart_dev[UART_NO_MAXS] = {RT_NULL};
uart_rx_manage rx_manage[UART_NO_MAXS] = {0};
char *uart_dev_name[UART_NO_MAXS] = {
#if defined(USR_USING_UART1)
    "uart1",
#endif
#if defined(USR_USING_UART2)
    "uart2",
#endif
#if defined(USR_USING_UART3)
    "uart3",
#endif
#if defined(USR_USING_UART4)
    "uart4",
#endif
#if defined(USR_USING_UART5)
    "uart5",
#endif
#if defined(USR_USING_UART6)
    "uart6",
#endif
#if defined(USR_USING_UART7)
    "uart7",
#endif
#if defined(USR_USING_UART8)
    "uart8",
#endif
};


rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size)
{
    uint8_t data;
    RT_UNUSED(size);

    for(int i = 0; i < UART_NO_MAXS; ++i) {
        if(dev == uart_dev[i]) {
            uart_rx_manage *rx = &rx_manage[i];

            // 接收回调只负责读取、保存数据和刷新时间，不在中断中判断或解析报文
            while(rt_device_read(dev, 0, &data, 1) == 1) {
                if(rx->len < UART_RX_BUF_SIZE) {
                    rx->rx_buf[rx->len++] = data;
                }
                else {
                    // 缓冲区满后继续读空驱动缓冲区，同时标记本帧溢出，等待线程统一丢弃
                    rx->overflow = RT_TRUE;
                }

                // 每收到一个新字节，都重新开始计算帧间超时时间
                rx->last_tick = rt_tick_get();
            }

            return RT_EOK;
        }
    }

    return -RT_ERROR;
}

// 接收缓冲区中有数据，并且最后一个字节后的静默时间达到 timeout_tick，则截取为完整帧
// 接收回调可能在中断中修改接收状态，因此复制和清空缓冲区时需要短暂关闭中断
static rt_bool_t uart_take_timeout_frame(uart_rx_manage *rx)
{
    rt_base_t level;
    rt_tick_t now;
    rt_bool_t frame_ready = RT_FALSE;

    level = rt_hw_interrupt_disable();
    now = rt_tick_get();

    if((rx->len > 0) && ((rt_tick_t)(now - rx->last_tick) >= rx->timeout_tick))
    {
        rx->frame_len = rx->len;
        rt_memcpy(rx->frame_buf, rx->rx_buf, rx->frame_len);
        rx->frame_overflow = rx->overflow;

        // 清空当前接收状态，后续字节从 rx_buf[0] 开始组成下一帧
        rx->len = 0;
        rx->overflow = RT_FALSE;
        frame_ready = RT_TRUE;
    }

    rt_hw_interrupt_enable(level);
    return frame_ready;
}

// 在线程中把按静默时间截取出的完整帧提交给轮询模块，避免在串口接收中断中解析Modbus
static void uart_dispatch_frame(uint16_t uart_no)
{
    uart_rx_manage *rx = &rx_manage[uart_no];

    if(rx->frame_overflow == RT_TRUE)
    {
        // 超长帧已经不完整，继续解析可能产生错误，因此直接丢弃
        rt_kprintf("uart%d rx frame overflow, frame dropped\n", uart_no);
        rx->frame_len = 0;
        rx->frame_overflow = RT_FALSE;
        return;
    }
    /* 轮询模块只在WAIT_RESPONSE状态接收该串口的帧，其他时刻返回忙并丢弃本帧。 */
    cycle_loop_rx_frame(uart_no,
                        rx->frame_buf,
                        (uint16_t)rx->frame_len);

    // 本帧处理结束后清除帧状态，frame_buf 的内容无需专门清零
    rx->frame_len = 0;
    rx->frame_overflow = RT_FALSE;
}

// 向指定串口写入数据，串口编号使用 UART1_NO、UART3_NO 等逻辑编号
rt_size_t uart_mgmt_write(uint16_t uart_no, const void *buffer, rt_size_t size)
{
    if((uart_no >= countof(uart_dev)) || (buffer == RT_NULL) || (size == 0))
    {
        return 0;
    }

    if(uart_dev[uart_no] == RT_NULL)
    {
        return 0;
    }

    return rt_device_write(uart_dev[uart_no], 0, buffer, size);
}

// 给指定串口注册完整帧处理函数，不同协议可分别绑定到不同串口
rt_err_t uart_mgmt_set_rx_app(uint16_t uart_no, uart_rx_app_t rx_app)
{
    if(uart_no >= countof(uart_dev))
    {
        return -RT_EINVAL;
    }

//    uart_dev[uart_no].rx_app = rx_app;
    return RT_EOK;
}

void get_uart_check(uint8_t check_bit, uart_check *p_check)
{
    switch(check_bit)
    {
    case UART_CHECK_8N1:
        p_check->uart_data_bits = DATA_BITS_8;
        p_check->uart_parity    = PARITY_NONE;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    case UART_CHECK_8O1:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity    = PARITY_ODD;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    case UART_CHECK_8E1:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity    = PARITY_EVEN;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    default:
        break;
    }
}

void uart_init(void)
{
    for(int i = 0; i < UART_NO_MAXS; ++i){
        // 将毫秒转换为系统节拍，低节拍系统中转换结果至少取 1 tick
        rx_manage[i].timeout_tick = rt_tick_from_millisecond(UART_FRAME_TIMEOUT_MS);
        if(rx_manage[i].timeout_tick == 0)
        {
            rx_manage[i].timeout_tick = 1;
        }

        uart_dev[i] = rt_device_find(uart_dev_name[i]);
        if(uart_dev[i] != RT_NULL){
            uart_check _uart_check = {0};
            get_uart_check(ctu_cfg.uart_check[i], &_uart_check);
            struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;  // 初始化配置参数
            config.baud_rate = ctu_cfg.uart_baud[i];
            config.data_bits = _uart_check.uart_data_bits;
            config.stop_bits = _uart_check.uart_stop_bits;
            config.bufsz     = 256;
            config.parity    = _uart_check.uart_parity;

            // step3：通过控制接口配置串口设备
            rt_device_control(uart_dev[i], RT_DEVICE_CTRL_CONFIG, &config);

            // step4：以中断接收及轮询发送模式打开串口设备
            rt_device_open(uart_dev[i], RT_DEVICE_FLAG_INT_RX);

            rt_device_set_rx_indicate(uart_dev[i], uart_rx_callback);
        }
    }
}


void uart_mgmt_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);

    while(1)
    {
        // 轮询各串口的帧间静默时间，完整帧判断和业务分发全部在线程中执行
        for(int i = 0; i < UART_NO_MAXS; ++i)
        {
            if((uart_dev[i] != RT_NULL) &&
               (uart_take_timeout_frame(&rx_manage[i]) == RT_TRUE))
            {
                uart_dispatch_frame((uint16_t)i);
            }
        }

        // 线程无需一直占用 CPU，1 ms 轮询周期足够覆盖当前超时配置
        rt_thread_mdelay(UART_RX_POLL_MS);
    }
}
