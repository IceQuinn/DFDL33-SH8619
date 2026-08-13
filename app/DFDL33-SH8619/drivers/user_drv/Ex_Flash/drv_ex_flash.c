/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-07     mutou       the first version
 */
#include "drv_ex_flash.h"
#include "spi_flash_sfud.h"
#include "drv_spi.h"

#define ZD25Q64_SPI_NAME             "spi2"
#define SPI_ZD25Q64_DEVICE_NAME      "spi20"
#define ZD25Q64_NSS_GPIO_PORT        GPIOB
#define ZD25Q64_NSS_GPIO_PIN         GPIO_PINS_12

/* 将ZD25Q64挂载在spi2上并配置 */
int32_t ex_flash_spi_init(void)
{
    /* 把spi20挂到spi2上 */
    if(RT_EOK != rt_hw_spi_device_attach(ZD25Q64_SPI_NAME, SPI_ZD25Q64_DEVICE_NAME, ZD25Q64_NSS_GPIO_PORT, ZD25Q64_NSS_GPIO_PIN))
    {
        rt_kprintf("spi2 bus attach ZD25Q64 device Failed !\n");
        return -RT_ERROR;
    }

    /* 在通用串行驱动中查找ZD25Q64设备 */
    if(RT_NULL == rt_sfud_flash_probe("W25Q64", SPI_ZD25Q64_DEVICE_NAME))
    {
        rt_kprintf("dev ZD25Q64 find error !\n");
        return -RT_ERROR;
    }
    rt_kprintf("ex_flash spi & sfdp init success !\n");
    return RT_EOK;
}


sfud_flash *sfud_dev = RT_NULL;

struct rt_mutex flash_handle;     //操作FLASH互斥量，同时只能有一个位置调用
#define FLX_MTX_NAME        "flh_mtx"

uint8_t flash_status = 0;   // FLASH状态，0正常，1故障
int32_t flash_init(void)
{
    ex_flash_spi_init();

    if(RT_EOK != rt_mutex_init(&flash_handle, FLX_MTX_NAME, RT_NULL))
    {
        rt_kprintf("%s init FAILED!!!\n", FLX_MTX_NAME);
        return -RT_ERROR;
    }

    sfud_dev = rt_sfud_flash_find(SPI_ZD25Q64_DEVICE_NAME); // 获取 sfud_dev
    if(RT_NULL == sfud_dev)
    {
        rt_kprintf("find ZD25Q64 FAILED!!!\n");
        return -RT_ERROR;
    }
    rt_kprintf("ex_flash drv init success !\n");
    return RT_EOK;
}

int32_t flash_write(uint32_t flashaddr, void* data, uint32_t len)
{
    rt_mutex_take(&flash_handle, RT_WAITING_FOREVER);
    sfud_err result = SFUD_SUCCESS;

    result = sfud_erase_write(sfud_dev, flashaddr, len, data);
    rt_mutex_release(&flash_handle);
    if(result != SFUD_SUCCESS)
    {
        rt_kprintf("sfud_read/write from zd25q64 error \n");
        return -RT_ERROR;
    }
    return RT_EOK;
}

int32_t flash_read(uint32_t flashaddr, void* data, uint32_t len)
{
    rt_mutex_take(&flash_handle, RT_WAITING_FOREVER);
    sfud_err result = SFUD_SUCCESS;

    result = sfud_read(sfud_dev, flashaddr, len, data);

    rt_mutex_release(&flash_handle);
    if(result != SFUD_SUCCESS)
    {
        rt_kprintf("read from zd25q64 error \n");
        return -RT_ERROR;
    }
    return RT_EOK;
}
