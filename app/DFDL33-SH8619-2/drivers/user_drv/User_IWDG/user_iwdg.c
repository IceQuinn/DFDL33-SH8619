#include "user_iwdg.h"
//#include <measurement.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "event_deal.h"
#include "ctu_cfg.h"
#include "user_rtc.h"


#define WDT_DEVICE_NAME    "wdt"    /* 看门狗设备名称 */
static rt_device_t wdg_dev; /* 看门狗设备句柄 */

int set_iwdg_timeout(int val)
{
    if(rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &val) !=RT_EOK)
    {
        rt_kprintf("Set iwdg timeout faied\n");
        return RT_ERROR;
    }
    return RT_EOK;
}

int user_iwdg_init(void)
{
    rt_uint32_t timeout = 10; /* 溢出时间，单位：秒*/

    /* 根据设备名称查找看门狗设备，获取设备句柄 */
    wdg_dev = rt_device_find(WDT_DEVICE_NAME);
    if (!wdg_dev)
    {
        rt_kprintf("find wdg_dev failed!\n");
        return RT_ERROR;
    }
    /* 初始化设备 */
    if (rt_device_init(wdg_dev) != RT_EOK)
    {
        rt_kprintf("wdg_dev init error\n");
    }

    /* 设置看门狗溢出时间 */
    if (rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout) != RT_EOK)
    {
        rt_kprintf("set wdg_dev timeout failed!\n");
        return RT_ERROR;
    }
    /* 启动看门狗 */
    if (rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL) != RT_EOK)
    {
        rt_kprintf("start wdg_dev failed!\n");
        return -RT_ERROR;
    }
    /* 设置空闲线程回调函数 */
    //rt_thread_idle_sethook(idle_hook);

    return 1;
}
INIT_COMPONENT_EXPORT(user_iwdg_init);      //此处修改为设备初始化之后，测试发现版级初始化不能够完成看门狗初始化

/* 停止喂狗标志位，0=正常喂狗，1=停止喂狗  */
int g_stop_wdg = 0;
void reboot(void)
{
//    DeviceStatusStruct.device_status_reboot = SET;
    /* 将事件转存至缓冲区 */
    Send_Event_To_Cache(ETP_REBOOT, SET, RT_NULL);
    rt_kprintf("stop watchdog, system will reboot!\n");
    g_stop_wdg = 1;
}
MSH_CMD_EXPORT(reboot, System reboot);

uint32_t g_iwdg_refresh_tick = 0;
uint32_t g_iwdg_refresh_time = 0;   //看门狗喂狗时间
uint32_t g_iwdg_refresh_maxtime = 0;//看门狗最长喂狗时间
//喂狗
void iwdg_entry(void)
{
//    return ;
    if (g_stop_wdg)
    {
//        rt_kprintf("stop watchdog, system will reboot!\n");
    }
    else
    {
        if(g_iwdg_refresh_tick == 0)
        {
            g_iwdg_refresh_tick = rt_tick_get();
        }
        else
        {
            g_iwdg_refresh_time = rt_tick_get() - g_iwdg_refresh_tick;
            if(g_iwdg_refresh_time > g_iwdg_refresh_maxtime)
            {
                g_iwdg_refresh_maxtime = g_iwdg_refresh_time;

            }
            g_iwdg_refresh_tick = rt_tick_get();
        }
        //HAL_IWDG_Refresh(&IwdgHandle);
        /* 喂狗 */
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
    }
}

