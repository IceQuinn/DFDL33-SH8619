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

#include <drv_common.h>
#include <rtdevice.h>

#include "ctu_cfg.h"
#include "cycle_loop.h"
#include "inv_data.h"

/* 最后一个字节之后连续10ms没有新数据时，将当前串口数据截取为一帧。 */
#define UART_RX_BUF_SIZE          256U
#define UART_FRAME_TIMEOUT_TICKS   10U
#define UART_RX_POLL_TICKS          1U

/* 串口数据位、停止位和校验位配置。 */
typedef struct Uart_Check {
    uint8_t uart_data_bits;
    uint8_t uart_stop_bits;
    uint8_t uart_parity;
} uart_check;

/* 单个串口使用双缓冲，接收回调写rx_buf，管理线程处理frame_buf。 */
typedef struct Uart_Rx_Manage {
    uint8_t rx_buf[UART_RX_BUF_SIZE];       /* 当前正在接收的数据。 */
    uint8_t frame_buf[UART_RX_BUF_SIZE];    /* 已按静默时间截取的完整帧。 */
    volatile rt_size_t len;                 /* rx_buf中已有的字节数。 */
    rt_size_t frame_len;                    /* frame_buf中的有效帧长度。 */
    volatile rt_tick_t last_tick;           /* 最后一次收到字节时的系统tick。 */
    rt_tick_t timeout_tick;                 /* 判断一帧结束所需的静默tick数。 */
    volatile rt_bool_t overflow;            /* 当前接收帧是否超过rx_buf容量。 */
    rt_bool_t frame_overflow;               /* 已截取帧是否发生过溢出。 */
} uart_rx_manage;

/* 串口设备对象、接收上下文及设备名称使用相同下标。 */
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

/* 串口接收回调只保存数据并刷新接收时间，不在中断上下文中解析业务报文。 */
rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size)
{
    uint16_t index;
    uint8_t data;

    RT_UNUSED(size);

    /* 遍历串口设备表，查找触发本次接收回调的串口。 */
    for(index = 0U; index < UART_NO_MAXS; ++index) {
        /* 当前设备与回调设备相同时处理该串口接收数据。 */
        if(dev == uart_dev[index]) {
            uart_rx_manage *rx = &rx_manage[index];

            /* 持续读空驱动接收缓冲区，确保一次回调中的全部字节都被取出。 */
            while(rt_device_read(dev, 0, &data, 1U) == 1U) {
                /* 接收缓冲区仍有空间时保存当前字节。 */
                if(rx->len < UART_RX_BUF_SIZE) {
                    rx->rx_buf[rx->len++] = data;
                }
                /* 缓冲区已满时继续读空驱动数据并标记当前帧溢出。 */
                else {
                    rx->overflow = RT_TRUE;
                }

                rx->last_tick = rt_tick_get(); /* 每收到一个字节都重新开始计算帧间静默时间。 */
            }

            return RT_EOK;
        }
    }

    return -RT_ERROR;
}

/* 静默时间达到配置值时，从接收缓冲区安全截取一帧到线程处理缓冲区。 */
static rt_bool_t uart_take_timeout_frame(uart_rx_manage *rx)
{
    rt_base_t level;
    rt_tick_t now;
    rt_bool_t frame_ready = RT_FALSE;

    level = rt_hw_interrupt_disable();
    now = rt_tick_get();

    /* 缓冲区有数据并且最后一个字节后的静默时间达到阈值时，确认收到完整帧。 */
    if((rx->len > 0U) && ((rt_tick_t)(now - rx->last_tick) >= rx->timeout_tick)) {
        rx->frame_len = rx->len;
        rt_memcpy(rx->frame_buf, rx->rx_buf, rx->frame_len);
        rx->frame_overflow = rx->overflow;
        rx->len = 0U;                   /* 后续字节从rx_buf[0]开始组成下一帧。 */
        rx->overflow = RT_FALSE;
        frame_ready = RT_TRUE;
    }

    rt_hw_interrupt_enable(level);
    return frame_ready;
}

/* 在线程中把完整串口帧提交给轮询模块，避免在接收中断中解析Modbus。 */
static void uart_dispatch_frame(uint16_t uart_no)
{
    uart_rx_manage *rx = &rx_manage[uart_no];
    rt_err_t result;

    /* 超长帧内容已经不完整，继续解析可能产生错误，因此直接丢弃。 */
    if(rx->frame_overflow == RT_TRUE) {
        rt_kprintf("[%08d] uart[%d] rx frame overflow, frame dropped\n", rt_tick_get(), uart_no);
        rx->frame_len = 0U;
        rx->frame_overflow = RT_FALSE;
        return;
    }

    /* 自动识别阶段优先接收报文，没有识别事务等待响应时再交给下行读写状态机。 */
    result = cycle_loop_rx_frame(uart_no, rx->frame_buf, (uint16_t)rx->frame_len);

    /* 自动识别没有接收本帧时，尝试提交给周期读取或实时控制使用的端口邮箱。 */
    if(result != RT_EOK) {
        Inv_Data_Rx_Frame(uart_no, rx->frame_buf, (uint16_t)rx->frame_len);
    }

    rx->frame_len = 0U;                 /* 本帧提交后释放frame_buf供下一帧使用。 */
    rx->frame_overflow = RT_FALSE;
}

/* 向指定逻辑串口写入数据，返回驱动实际写入的字节数。 */
rt_size_t uart_mgmt_write(uint16_t uart_no, const void *buffer, rt_size_t size)
{
    /* 串口号越界、数据指针为空或长度为0时不调用设备驱动。 */
    if((uart_no >= countof(uart_dev)) || (buffer == RT_NULL) || (size == 0U)) {
        return 0U;
    }

    /* 串口设备未初始化成功时不能发送数据。 */
    if(uart_dev[uart_no] == RT_NULL) {
        return 0U;
    }

    return rt_device_write(uart_dev[uart_no], 0, buffer, size);
}

/* 预留串口业务回调注册接口，当前轮询模块直接通过cycle_loop_rx_frame接收帧。 */
rt_err_t uart_mgmt_set_rx_app(uint16_t uart_no, uart_rx_app_t rx_app)
{
    RT_UNUSED(rx_app);

    /* 串口号超出设备表范围时返回参数错误。 */
    if(uart_no >= countof(uart_dev)) {
        return -RT_EINVAL;
    }

    return RT_EOK;
}

/* 将工程串口校验配置转换为RT-Thread串口驱动配置。 */
void get_uart_check(uint8_t check_bit, uart_check *p_check)
{
    /* 输出配置指针为空时不能写入转换结果。 */
    if(p_check == RT_NULL) {
        return;
    }

    /* 根据工程配置选择数据位、停止位和奇偶校验方式。 */
    switch(check_bit) {
    case UART_CHECK_8N1:
        p_check->uart_data_bits = DATA_BITS_8;
        p_check->uart_parity = PARITY_NONE;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;

    case UART_CHECK_8O1:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity = PARITY_ODD;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;

    case UART_CHECK_8E1:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity = PARITY_EVEN;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;

    default:
        break;
    }
}

/* 查找并初始化工程启用的全部串口设备。 */
void uart_init(void)
{
    uint16_t index;

    /* 所有串口依次设置接收超时、通信参数和接收回调。 */
    for(index = 0U; index < UART_NO_MAXS; ++index) {
        uart_check uart_config = {0};
        struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

        rx_manage[index].timeout_tick = UART_FRAME_TIMEOUT_TICKS; /* 当前系统1tick等于1ms，无需换算。 */
        uart_dev[index] = rt_device_find(uart_dev_name[index]);

        /* 只有成功查找到设备对象的串口才执行后续配置和打开操作。 */
        if(uart_dev[index] != RT_NULL) {
            get_uart_check(ctu_cfg.uart_check[index], &uart_config);
            config.baud_rate = ctu_cfg.uart_baud[index];
            config.data_bits = uart_config.uart_data_bits;
            config.stop_bits = uart_config.uart_stop_bits;
            config.bufsz = UART_RX_BUF_SIZE;
            config.parity = uart_config.uart_parity;
            rt_device_control(uart_dev[index], RT_DEVICE_CTRL_CONFIG, &config);
            rt_device_open(uart_dev[index], RT_DEVICE_FLAG_INT_RX);
            rt_device_set_rx_indicate(uart_dev[index], uart_rx_callback);
        }
    }
}

/* 串口管理线程按1ms周期截取并分发三个轮询串口的完整响应帧。 */
void uart_mgmt_thread_entry(void *parameter)
{
    uint16_t index;

    RT_UNUSED(parameter);

    /* 串口管理线程持续运行并处理所有已初始化串口。 */
    while(1) {
        /* 每个串口独立判断帧间静默时间，不会等待其他串口完成。 */
        for(index = 0U; index < UART_NO_MAXS; ++index) {
            /* 串口设备存在并且截取到完整帧时，将帧提交给轮询模块。 */
            if((uart_dev[index] != RT_NULL) && (uart_take_timeout_frame(&rx_manage[index]) == RT_TRUE)) {
                uart_dispatch_frame(index);
            }
        }

        rt_thread_mdelay(UART_RX_POLL_TICKS); /* 1ms轮询周期足够覆盖当前10ms帧间超时。 */
    }
}
