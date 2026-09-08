#ifndef __DLT645_DEAL_H__
#define __DLT645_DEAL_H__

#include <stdio.h>
#include <rtthread.h>

#define DL645_ADDR_SIZE     6

/* 645接收报文回调函数 */
void dlt645_rx_callback(void *ptr, uint16_t len, uint16_t buf_source);
void Dlt645_Init(void);
void dlt645_deal(uint8_t uart_no, uint8_t *dlt645_addr, uint8_t *bufPtr, uint16_t PackLen);

rt_err_t dlt645_data_ack(uint16_t uart_no, const void *buffer, rt_size_t size);

void dlt645_deal_thread_entry(void* parameter);

#endif
