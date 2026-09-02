#include "HJ02C.h"
#include "drv_hj02c.h"
#include <string.h>
#include <stdio.h>

static void *memmem(const void *haystack, size_t hlen,
                    const void *needle, size_t nlen)
{
    if (nlen == 0 || hlen < nlen) return RT_NULL;

    const uint8_t *h = (const uint8_t *)haystack;
    const uint8_t *n = (const uint8_t *)needle;

    for (size_t i = 0; i <= hlen - nlen; i++)
    {
        if (memcmp(h + i, n, nlen) == 0)
            return (void *)(h + i);
    }
    return RT_NULL;
}

rt_size_t hj02c_spi_send_Test(struct rt_spi_device *device, const void *buf, rt_size_t len)
{
    rt_size_t sent;

    sent = rt_spi_send(device, buf, len);
    return sent;
}

void HJ02C_Test(uint8_t argc, char **argv)
{
    if(wait_irq_high(2000) != RT_EOK)
    {
//        rt_mutex_release(spi_lock);
        rt_kprintf("wait_irq_high(2000)\n");
        return -1;
    }

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);

    uint32_t len = strlen(argv[1]);

    hj02c_spi_send_Test(&g_hj02c_dev, argv[1], len);

    rt_hw_us_delay(2);
    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);

    if(wait_irq_low(2000) != RT_EOK)
    {
//        rt_mutex_release(spi_lock);
        rt_kprintf("wait_irq_low(2000)\n");
        return -1;
    }

    uint8_t dummy = 0xFF;
    uint8_t resp[256] = {0};

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);

    rt_spi_transfer(&g_hj02c_dev, &dummy, &len, 2);

    rt_spi_transfer(&g_hj02c_dev, &dummy, resp, len);

    if (len == 0) {
        rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
        rt_kprintf("len == 0\n");
        return ;
    }
    rt_kprintf("len == %d\n", len);

    rt_hw_us_delay(1);

    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);

    rt_kprintf("%s\n", resp);
}
MSH_CMD_EXPORT(HJ02C_Test, HJ02C_Test);

/* 发送指令 + 读取应答 */
rt_err_t hj02c_send_cmd_and_get_resp(const char *cmd, char *resp,
                                         uint16_t max_len, uint16_t *out_len)
{
    uint8_t cmd_len = strlen(cmd);
    uint16_t len = 0;
    uint8_t dummy = 0xFF;

    rt_mutex_take(spi_lock, RT_WAITING_FOREVER);

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);

    rt_spi_send(&g_hj02c_dev, cmd, cmd_len);

    rt_hw_us_delay(2);
    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);

    rt_tick_t start = rt_tick_get();
    while (rt_pin_read(HJ02C_IRQ_PIN) != PIN_LOW) {
        if (rt_tick_get() - start > rt_tick_from_millisecond(2000)) {
            rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
            rt_mutex_release(spi_lock);
            return RT_ETIMEOUT;
        }
        rt_thread_mdelay(1);
    }

    rt_pin_write(HJ02C_CS_PIN, PIN_LOW);
    rt_hw_us_delay(2);

    rt_spi_transfer(&g_hj02c_dev, &dummy, &len, 2);

    if (len == 0 || len > max_len) {
        rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
        rt_mutex_release(spi_lock);
        return RT_ERROR;
    }

    rt_spi_transfer(&g_hj02c_dev, &dummy, resp, len);

    rt_hw_us_delay(1);

    rt_pin_write(HJ02C_CS_PIN, PIN_HIGH);
    rt_mutex_release(spi_lock);

    wait_irq_high(500);

    *out_len = len;
    return RT_EOK;
}

rt_err_t hj02c_read_cmd(const char *cmd, char *ver, uint16_t max_len)
{
    char resp[64]; uint16_t len;
    rt_err_t err = hj02c_send_cmd_and_get_resp(cmd, resp, sizeof(resp), &len);
    if (err) return err;
    char *p = memchr(resp, '<', len);
    if (!p) return RT_ERROR;
//    uint16_t vlen = len - (p - resp + 1) - 1;
    uint16_t vlen = len;
    if (vlen >= max_len) vlen = max_len - 1;
    memcpy(ver, p, vlen);
    ver[vlen] = '\0';
    return RT_EOK;
}

rt_err_t hj02c_send_set_cmd(const char *cmd)
{
    if (!cmd) return RT_ERROR;

    char resp[32];
    uint16_t len = 0;

    rt_err_t err = hj02c_send_cmd_and_get_resp(cmd, resp, sizeof(resp), &len);
    if (err != RT_EOK) {
        return err;
    }

    return memmem(resp, len, "=ok>", 4) ? RT_EOK : RT_ERROR;
}


/* 设置名字 */
rt_err_t hj02c_set_name(const char *name)
{
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "<ST_NAME=%s>", name);
    return hj02c_send_set_cmd(cmd);
}

/* 设置广播间隔 */
rt_err_t hj02c_set_adv_gap(uint16_t gap_ms)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "<ST_ADV_GAP=%u>", gap_ms);
    return hj02c_send_set_cmd(cmd);
}

/* 开关广播 */
rt_err_t hj02c_set_adv(uint8_t on)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "<ST_ADV_ONOFF=%d>", on ? 1 : 0);
    return hj02c_send_set_cmd(cmd);
}

/* 最小连接间隔 */
rt_err_t hj02c_set_con_min_gap(uint16_t ms)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "<ST_CON_MIN_GAP=%u>", ms * 10);
    return hj02c_send_set_cmd(cmd);
}

/* 最大连接间隔 */
rt_err_t hj02c_set_con_max_gap(uint16_t ms)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "<ST_CON_MAX_GAP=%u>", ms * 10);
    return hj02c_send_set_cmd(cmd);
}

/* 设置发射功率 */
rt_err_t hj02c_set_tx_power(int8_t dbm)
{
    char cmd[32];
    if (dbm == 12)
        snprintf(cmd, sizeof(cmd), "<ST_TX_POWER_MAX=+12>");
    else
        snprintf(cmd, sizeof(cmd), "<ST_TX_POWER=%+d>", dbm);
    return hj02c_send_set_cmd(cmd);
}

/* 开关高速模式 */
rt_err_t hj02c_set_high_speed(uint8_t on)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "<ST_HIGH_SPEED=%d>", on ? 1 : 0);
    return hj02c_send_set_cmd(cmd);
}

/* 复位 */
rt_err_t hj02c_reset(void)
{
    return hj02c_send_set_cmd("<ST_RESET_BLE>");
}
MSH_CMD_EXPORT(hj02c_reset, hj02c_reset);

rt_err_t hj02c_factory(void)
{
    return hj02c_send_set_cmd("<ST_FACTORY=1>");
}

/* 初始化 */
rt_err_t hj02c_basic_init(const char *device_name)
{
    char ver[64];

    hj02c_spi4_init();

    rt_thread_mdelay(100); // 等待 SPI4 初始化完成

    if (hj02c_factory() != RT_EOK)
        return RT_ERROR;
    rt_thread_mdelay(500);

    if(hj02c_read_cmd("<RD_SOFT_VERSION>", ver, sizeof(ver)) != RT_EOK)
        return RT_ETIMEOUT;
    rt_kprintf("[HJ02C] reply data: %s\n", ver);

//    if(hj02c_read_cmd("<RD_CLIENT_LINK>", ver, sizeof(ver)) != RT_EOK)
//        return RT_ETIMEOUT;
//    rt_kprintf("[HJ02C] reply data: %s\n", ver);

    if (hj02c_set_name(device_name) != RT_EOK)
        return RT_ERROR;
    rt_thread_mdelay(500);

    HJ02C_IRQ_irq_enable();
    rt_kprintf("[HJ02C] Basic init done, advertising...\n");
    return RT_EOK;
}

extern rt_sem_t irq_sem;

void hj02c_rx_thread_entry(void *parameter)
{
    uint16_t len;
    uint8_t rx_buf[512];

    /* 初始化蓝牙模块 */
    if(hj02c_basic_init("MyAT32_001") == RT_EOK)
    {
        rt_kprintf("BLE is ready!\n");
    }

    while (1)
    {
        if (rt_sem_take(irq_sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            len = hj02c_spi_recv(&g_hj02c_dev, rx_buf);
            rt_kprintf("recv len = %d\r\n", len);
        }
    }
}

