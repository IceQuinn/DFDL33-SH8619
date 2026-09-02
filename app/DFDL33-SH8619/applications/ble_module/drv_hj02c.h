#ifndef __DRV_HJ02C_H__
#define __DRV_HJ02C_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>
#include <drv_common.h>

#define HJ02C_SPI_BUS_NAME    "spi4"
#define HJ02C_SPI_DEV_NAME    "spi40"

#define HJ02C_CS_PIN          GET_PIN(A, 4)   // 软件 CS
#define HJ02C_IRQ_PIN         GET_PIN(A, 5)   // IRQ，下降沿触发，低电平有效
#define HJ02C_RST_PIN         GET_PIN(A, 6)   // 复位，高电平不复位

extern struct rt_spi_device g_hj02c_dev;
extern rt_mutex_t spi_lock;

int hj02c_spi4_init(void);
void HJ02C_IRQ_irq_enable(void);
rt_err_t wait_irq_high(uint32_t timeout_ms);
rt_err_t wait_irq_low(uint32_t timeout_ms);
rt_size_t hj02c_spi_send(struct rt_spi_device *device, const void *buf, rt_size_t len);
rt_size_t hj02c_spi_recv(struct rt_spi_device *device, void *buf);

#endif

