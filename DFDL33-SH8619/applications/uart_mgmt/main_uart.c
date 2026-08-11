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
#include <drivers/serial.h>

#include "ctu_cfg.h"

typedef struct
{
    uint8_t uart_data_bits;
    uint8_t uart_stop_bits;
    uint8_t uart_parity;
}uart_check;

typedef struct
{
    rt_device_t dev;                    //串口设备
    char     *uart_name;                //串口名称
    uint16_t *uart_protocol;
    rt_err_t (*rx_app)(void *ptr, uint16_t len, uint16_t buf_type, uint16_t protocol); //报文处理回调函数
    uint16_t buf_type;                  //数据类型
    uint16_t ctrl_pin;                  //控制引脚
    uint32_t *uart_baud;                //波特率
    uint16_t *uart_check;               //校验格式
    uint16_t oflag;                     //收发模式
}uart_device;

uart_device uart_dev[UART_NO_MAXS] = {
        {RT_NULL, UART1_NAME, &ctu_cfg.uart_protocol[UART1_NO], RT_NULL, RS485_UART_NO_1, 0, &ctu_cfg.uart_baud[UART1_NO], &ctu_cfg.uart_check[UART1_NO], RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_DMA_TX},
        {RT_NULL, UART3_NAME, &ctu_cfg.uart_protocol[UART3_NO], RT_NULL, WL_UART_NO, 0, &ctu_cfg.uart_baud[UART3_NO], &ctu_cfg.uart_check[UART3_NO], RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_DMA_TX},
//        {RT_NULL, UART6_NAME, &ctu_cfg.uart_protocol[UART6_NO], mb_queue_rx_send, RS485_UART_NO_2, 0, &ctu_cfg.uart_baud[UART6_NO], &ctu_cfg.uart_check[UART6_NO], RT_DEVICE_FLAG_DMA_RX | RT_DEVICE_FLAG_DMA_TX}
};


rt_err_t uart_rx_callback(rt_device_t dev, rt_size_t size)
{

}

void get_uart_check(uint8_t check_bit, uart_check *p_check)
{
    switch(check_bit)
    {
    case 0:
        p_check->uart_data_bits = DATA_BITS_8;
        p_check->uart_parity = PARITY_NONE;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    case 1:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity = PARITY_ODD;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    case 2:
        p_check->uart_data_bits = DATA_BITS_9;
        p_check->uart_parity = PARITY_EVEN;
        p_check->uart_stop_bits = STOP_BITS_1;
        break;
    default:
        break;
    }
}

void uart_init(void)
{
    for(int i = 0; i < countof(uart_dev); ++i)
    {
        uart_dev[i].dev = rt_device_find(uart_dev[i].uart_name);
        if(uart_dev[i].dev != RT_NULL)
        {
            uart_check _uart_check = {0};
            get_uart_check(ctu_cfg.uart_check[i], &_uart_check);
            struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;  /* 初始化配置参数 */
            config.baud_rate = ctu_cfg.uart_baud[i];
            config.data_bits = _uart_check.uart_data_bits;
            config.stop_bits = _uart_check.uart_stop_bits;
            config.bufsz     = 256;
            config.parity    = _uart_check.uart_parity;

            /* step3：控制串口设备。通过控制接口传入命令控制字，与控制参数 */
            rt_device_control(uart_dev[i].dev, RT_DEVICE_CTRL_CONFIG, &config);

            /* step4：打开串口设备。以中断接收及轮询发送模式打开串口设备 */
            rt_device_open(uart_dev[i].dev, uart_dev[i].oflag);

            rt_device_set_rx_indicate(uart_dev[i].dev, uart_rx_callback);
//            uart_tim_create(uart_dev[i].dev);
        }
    }
}


void uart_mgmt_thread_entry(void *parameter)
{
    while(1)
    {
        rt_thread_mdelay(1000);
    }
}
