/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-10     mutou       the first version
 */
#ifndef APPLICATIONS_UART_MGMT_MAIN_UART_H_
#define APPLICATIONS_UART_MGMT_MAIN_UART_H_

#include <rtthread.h>
#include <drv_common.h>

typedef rt_err_t (*uart_rx_app_t)(uint16_t uart_no, void *ptr, uint16_t len, uint16_t buf_type, uint16_t protocol);

/* 查找并初始化工程启用的全部串口设备。 */
void uart_init(void);

/* 串口管理线程按帧间静默时间截取并分发完整报文。 */
void uart_mgmt_thread_entry(void *parameter);

/* 向指定逻辑串口写入数据并返回驱动实际写入长度。 */
rt_size_t uart_mgmt_write(uint16_t uart_no, const void *buffer, rt_size_t size);

/* 预留串口业务回调注册接口。 */
rt_err_t uart_mgmt_set_rx_app(uint16_t uart_no, uart_rx_app_t rx_app);

#endif /* APPLICATIONS_UART_MGMT_MAIN_UART_H_ */
