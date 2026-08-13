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

void uart_init(void);
void uart_mgmt_thread_entry(void *parameter);
rt_size_t uart_mgmt_write(uint16_t uart_no, const void *buffer, rt_size_t size);
rt_err_t uart_mgmt_set_rx_app(uint16_t uart_no, uart_rx_app_t rx_app);

#endif /* APPLICATIONS_UART_MGMT_MAIN_UART_H_ */
