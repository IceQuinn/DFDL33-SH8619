/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-18     mutou       the first version
 */

#include "user_rtc.h"
#include <rtthread.h>
#include <rtdevice.h>
#include "at32f403a_407.h"


// 初始化RTC日期时间
int rtc_reinit(void)
{
    rt_err_t ret = RT_EOK;

    /* 设置日期 */
    ret = set_date(2020, 12, 19);
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC date failed\n");
        return ret;
    }

    /* 设置时间 */
    ret = set_time(9, 30, 45);
    if (ret != RT_EOK)
    {
        rt_kprintf("set RTC time failed\n");
        return ret;
    }
    return ret;
}
//MSH_CMD_EXPORT(rtc_init, rtc init);

//void rtc_init(void)
//{
//    if(time(RT_NULL) < 24*60*60)
//    {
//        rt_kprintf("time error %d\n", time(RT_NULL));
//        set_date(2022, 1, 1);
//        set_time(0, 0, 0);
//    }
//}


//显示当前时间
void show_rtc_time(void)
{
    time_t now;
    now = time(RT_NULL);

    struct tm tm_info = {0};

    localtime_r(&now, &tm_info);
    //年份需要减100，月份需要加1
    rt_kprintf("Time:%04d-%02d-%02d %02d:%02d:%02d\n", tm_info.tm_year+1900, tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}
MSH_CMD_EXPORT(show_rtc_time, show local time);

// 获取月份
uint8_t get_month(void)
{
    time_t now;
    now = time(RT_NULL);

    struct tm tm_info = {0};

    localtime_r(&now, &tm_info);
    return (uint8_t )tm_info.tm_mon + 1;
}

uint32_t ertc_sub_second_get(void)
{
    uint32_t cnt;
    cnt = (RTC->divcnth & 0x0F) << 16;  // 只取高4位有效
    cnt |= (RTC->divcntl & 0xFFFF);      // 取低16位有效
    return cnt;
}

#define RTC_PREDIV  39999   // LICK预分频系数，固定值
uint16_t get_rtc_ms(void)
{
    uint32_t ss = ertc_sub_second_get();
    return (uint16_t)((RTC_PREDIV - ss) * 1000 / RTC_PREDIV);
}

// 设置年,2025
uint16_t set_year(uint16_t year)
{
    if((year < 2000) || (2099 < year))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_date(year, tm_info.tm_mon+1, tm_info.tm_mday))
    {
        rt_kprintf("set RTC date failed\n");
        return 1;
    }
    return 0;

}
//-100+2000，例如2021年读取读取出来的数据是121，要先减100，得出年份数，再加上2000得到实际年份
uint16_t set_month(uint16_t month)
{
    if((month < 1) || (12 < month))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_date(tm_info.tm_year-100 + 2000, month, tm_info.tm_mday))
    {
        rt_kprintf("set RTC date failed\n");
        return 1;
    }
    return 0;

}
uint16_t set_day(uint16_t day)
{
    if((day < 1) || (31 < day))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_date(tm_info.tm_year-100+2000, tm_info.tm_mon+1, day))
    {
        rt_kprintf("set RTC date failed\n");
        return 1;
    }
    return 0;
}
uint16_t set_hour(uint16_t hour)
{
    if((hour < 0) || (23 < hour))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_time(hour, tm_info.tm_min, tm_info.tm_sec))
    {
        rt_kprintf("set RTC time failed\n");
        return 1;
    }
    return 0;

}
uint16_t set_min(uint16_t min)
{
    if((min < 0) || (59 < min))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_time(tm_info.tm_hour, min, tm_info.tm_sec))
    {
        rt_kprintf("set RTC time failed\n");
        return 1;
    }
    return 0;

}
uint16_t set_sec(uint16_t sec)
{
    if((sec < 0) || (59 < sec))
    {
        return 1;
    }
    //获取时间
    time_t now;
    now = time(RT_NULL);
    struct tm tm_info = {0};
    localtime_r(&now, &tm_info);
    //设置时间
    if(RT_EOK != set_time(tm_info.tm_hour, tm_info.tm_min, sec))
    {
        rt_kprintf("set RTC time failed\n");
        return 1;
    }
    return 0;
}

//通过设置时间戳来设置时间
void set_timestamp(time_t sec)
{
    rt_device_t device;

//    sec = sec + 3600*8;   // 时区控制

    device = rt_device_find("rtc");
    if (device == RT_NULL)
    {
        rt_kprintf("set time failed!!\n");
        return ;
    }

    /* update to RTC device. */
    rt_device_control(device, RT_DEVICE_CTRL_RTC_SET_TIME, &sec);
}

struct tm get_local_time_t(void)
{
    struct tm self_local_time;

    time_t now;
    now = time(RT_NULL);

    localtime_r(( time_t *)&now, &self_local_time);

    //rt_kprintf("%02d/%02d %02d:%02d:%02d> ", self_local_time.tm_mon+1, self_local_time.tm_mday, self_local_time.tm_hour, self_local_time.tm_min, self_local_time.tm_sec);
    return self_local_time;
}

#if 0
void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */
      __HAL_RCC_PWR_CLK_ENABLE();//使能电源时钟PWR
      HAL_PWR_EnableBkUpAccess();//取消备份区域写保护
  /* USER CODE END RTC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
//      Error_Handler();
    }
    HAL_RTCEx_EnableBypassShadow(rtcHandle);
    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

#endif
