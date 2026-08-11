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
#include "uart_def.h"

void uart_init(void);
void uart_mgmt_thread_entry(void *parameter);

#endif /* APPLICATIONS_UART_MGMT_MAIN_UART_H_ */
