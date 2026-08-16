/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_
#define APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_

#include <stdint.h>
#include <rtthread.h>

#include "inverter_archive.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 通信接收层调用本接口提交一帧完整响应，三个扫描串口分别使用独立接收邮箱。 */
rt_err_t cycle_loop_rx_frame(uint16_t uart_no,
                             const uint8_t *frame,
                             uint16_t frame_len);

/* 线程入口：档案数为0时先扫描三个串口的地址，收到回复的端口再进入协议识别。 */
void cycle_loop_thread_entry(void *parameter);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_CYCLE_LOOP_H_ */
