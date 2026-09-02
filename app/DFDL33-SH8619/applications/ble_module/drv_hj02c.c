#include "drv_hj02c.h"

#include "drv_common.h"
#include "at32f403a_407.h"

struct rt_spi_device g_hj02c_dev;
rt_mutex_t spi_lock;
rt_sem_t irq_sem;

/* IRQ 中断回调 */
uint8_t callback_num = 0;
static void hj02c_irq_callback(void *args)
{
    if (irq_sem != RT_NULL)
    {
        rt_sem_release(irq_sem);
        callback_num++;
    }
}

void spi4_init(void)
{
    crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI4_PERIPH_CLOCK, TRUE);

    gpio_init_type gpio_init_struct;
    spi_init_type spi_init_struct;

    gpio_default_para_init(&gpio_init_struct);
    spi_default_para_init(&spi_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_7;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_8;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_9;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    gpio_pin_remap_config(SPI4_GMUX_0010, TRUE);

    /* configure param */
    spi_init_struct.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX;
    spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
    spi_init_struct.frame_bit_num = SPI_FRAME_8BIT;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.mclk_freq_division = SPI_MCLK_DIV_128;
    spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_LOW;
    spi_init_struct.clock_phase = SPI_CLOCK_PHASE_1EDGE;
    spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
    spi_init(SPI4, &spi_init_struct);

    spi_enable(SPI4, TRUE);
}


/* SPI4 设备初始化 */
int hj02c_spi4_init(void)
{
    spi4_init();

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

    rt_pin_mode(HJ02C_CS_PIN,  PIN_MODE_OUTPUT);
    rt_pin_mode(HJ02C_IRQ_PIN, PIN_MODE_INPUT_PULLUP);
    rt_pin_mode(HJ02C_RST_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);     // CS 默认不选中
    rt_pin_write(HJ02C_RST_PIN, PIN_HIGH);    // 模块正常工作

    rt_err_t ret = rt_spi_bus_attach_device(&g_hj02c_dev, "spi40", "spi4", RT_NULL);
    if (ret != RT_EOK) {
        rt_kprintf("[INIT] attach FAILED! ret=%d\n", ret);
        return -RT_ERROR;
    }

    struct rt_spi_configuration cfg;
    cfg.mode       = RT_SPI_MASTER | RT_SPI_MODE_0 | RT_SPI_MSB | RT_SPI_NO_CS;
    cfg.max_hz     = 500000;
    cfg.data_width = 8;
    rt_spi_configure(&g_hj02c_dev, &cfg);

    irq_sem  = rt_sem_create("hj_irq", 0, RT_IPC_FLAG_FIFO);
    spi_lock = rt_mutex_create("hj_spi", RT_IPC_FLAG_FIFO);

    return RT_EOK;
}

void HJ02C_IRQ_irq_enable(void)
{
    rt_pin_attach_irq(HJ02C_IRQ_PIN, PIN_IRQ_MODE_FALLING, hj02c_irq_callback, RT_NULL);
    rt_pin_irq_enable(HJ02C_IRQ_PIN, PIN_IRQ_ENABLE);
}

/* 等待 IRQ 恢复高 */
rt_err_t wait_irq_high(uint32_t timeout_ms)
{
    uint32_t start = rt_tick_get();
    while (rt_pin_read(HJ02C_IRQ_PIN) != PIN_HIGH)
    {
        if (rt_tick_get() - start > rt_tick_from_millisecond(timeout_ms))
            return -RT_ETIMEOUT;
        rt_thread_mdelay(1);
    }
    return RT_EOK;
}

/* 等待 IRQ 拉低 */
rt_err_t wait_irq_low(uint32_t timeout_ms)
{
    uint32_t start = rt_tick_get();
    while (rt_pin_read(HJ02C_IRQ_PIN) != PIN_LOW)
    {
        if (rt_tick_get() - start > rt_tick_from_millisecond(timeout_ms))
            return -RT_ETIMEOUT;
        rt_thread_mdelay(1);
    }
    return RT_EOK;
}

/* SPI 发送 */
rt_size_t hj02c_spi_send(struct rt_spi_device *device, const void *buf, rt_size_t len)
{
    rt_size_t sent;
    rt_mutex_take(spi_lock, RT_WAITING_FOREVER);

    if(wait_irq_high(2000) != RT_EOK)
    {
        rt_mutex_release(spi_lock);
        return -1;
    }

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);
    sent = rt_spi_send(device, buf, len);
    rt_hw_us_delay(2);
    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
    rt_mutex_release(spi_lock);
    return sent;
}

void hj02c_send_test(void)
{
    uint8_t tx_data[5] = {2,3,4,5,6};
    hj02c_spi_send(&g_hj02c_dev, tx_data, 5);
}
MSH_CMD_EXPORT(hj02c_send_test, hj02c_send_test);


/* SPI 接收 */
rt_size_t hj02c_spi_recv(struct rt_spi_device *device, void *buf)
{
//    uint8_t rcvd1;
//    uint8_t rcvd2;
    uint16_t rcvd;
    uint8_t dummy = 0xFF;
    rt_mutex_take(spi_lock, RT_WAITING_FOREVER);

    if(wait_irq_low(2000) != RT_EOK)
    {
        rt_mutex_release(spi_lock);
        return -1;
    }

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);

//    rt_spi_transfer(device, &dummy, &rcvd1, 1);
//    rt_spi_transfer(device, &dummy, &rcvd2, 1);
//    rt_kprintf("rcvd1 = %d, rcvd2 = %d\n", rcvd1, rcvd2);
//    rcvd = (rcvd2 << 8) | rcvd1;

    rt_spi_transfer(device, &dummy, &rcvd, 2);

    if (rcvd > 0 && rcvd <= 1024)
    {
        rt_spi_transfer(device, &dummy, buf, rcvd);
//        rt_kprintf("[HJ02C RX] rcvd=%d\n", rcvd);
    }

    rt_hw_us_delay(2);   // 等 SPI 硬件空闲再拉高 CS
    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
    rt_mutex_release(spi_lock);

    wait_irq_high(500);  // 等 IRQ 恢复高

    return rcvd;
}

