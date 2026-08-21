/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-27     IP155       the first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include "drv_common.h"

#include "led.h"
#include <string.h>
#include <stdlib.h>

// 请根据使用的内核或操作系统决定
#define OS_RTTHREAD     1   // 基于RT-Thread操作系统
#define OS_KEIL_NANO    0   // 基于KEIL库函数版本
#define OS_KEIL_HAL     0   // 基于KEIL HAL库函数版本
#define OS_OPENHARMONY  0   // 基于OpenHarmony操作系统
#if OS_RTTHREAD
#include "drv_common.h"
#endif

#if OS_OPENHARMONY
#include "los_task.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include "ohos_init.h"
#include "unistd.h"
#endif

#define DELAY_TICK      125

struct LED_PIN
{
    gpio_type *gpio_x;
    uint16_t  pins;
    const char* led_type;
};

struct LED_MODE
{
    uint32_t LED_TimeOut;   // 超时熄灭时间
    uint8_t  LED_Work_Sta;  // 灯状态
    uint8_t  LED_Work_Mode; // 工作模式
    struct LED_PIN  *LED_Pin;
};

//GPIO对应灯
struct LED_PIN  LED_PIN_Str[LED_ALL]  = {
        {GPIOC, GPIO_PINS_1 , "LED_RUN"  },
        {GPIOA, GPIO_PINS_0 , "LED_WH_TX"  },
        {GPIOA, GPIO_PINS_1 , "LED_WH_RX"  },
        {GPIOD, GPIO_PINS_0 , "LED_UP_TX"  },
        {GPIOD, GPIO_PINS_1 , "LED_UP_RX"  },
        {GPIOB, GPIO_PINS_0 , "LED_DN_TX"  },
        {GPIOB, GPIO_PINS_1 , "LED_DN_RX"  },
};

//LED灯配置
struct LED_MODE LED_MODE_Str[LED_ALL] = {
        {-1, 0, LED_NORMAL,     &LED_PIN_Str[LED_RUN]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_WH_TX]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_WH_RX]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_UP_TX]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_UP_RX]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_DN_TX]  },
        {-1, 0, LED_OFF,        &LED_PIN_Str[LED_DN_RX]  },
};

struct LED_MODE Last_LED_MODE_Str[LED_ALL] = {0};


/* LED灯对应GPIO初始化 */
void LED_GPIO_Init()
{
      gpio_init_type gpio_init_struct;

      crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
      crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
      crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
      crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);

      gpio_default_para_init(&gpio_init_struct);

      gpio_bits_reset(GPIOD, GPIO_PINS_0 | GPIO_PINS_1);
      gpio_bits_reset(GPIOC, GPIO_PINS_1);
      gpio_bits_reset(GPIOA, GPIO_PINS_0 | GPIO_PINS_1);
      gpio_bits_reset(GPIOB, GPIO_PINS_0 | GPIO_PINS_1);

      gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
      gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
      gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
      gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1;
      gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
      gpio_init(GPIOD, &gpio_init_struct);

      gpio_init_struct.gpio_pins = GPIO_PINS_1;
      gpio_init(GPIOC, &gpio_init_struct);

      gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1;
      gpio_init(GPIOA, &gpio_init_struct);

      gpio_init_struct.gpio_pins = GPIO_PINS_0 | GPIO_PINS_1;
      gpio_init(GPIOB, &gpio_init_struct);
}


/* LED灯GPIO电平控制 */
int8_t LED_GPIO_Set(struct LED_PIN led_type, uint8_t Level)
{
    if(NULL == led_type.gpio_x)
    {
        return -1;
    }

    if(Level)
    {
        //gpio_bits_write(led_type.gpio_x, led_type.pins, TRUE);
        gpio_bits_set(led_type.gpio_x, led_type.pins);
    }
    else
    {
        //gpio_bits_write(led_type.gpio_x, led_type.pins, FALSE);
        gpio_bits_reset(led_type.gpio_x, led_type.pins);
    }
    return 0;
}


/* LED灯初始化 */
void LED_Init(void)
{
    LED_GPIO_Set(*LED_MODE_Str[LED_RUN].LED_Pin     , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_WH_TX].LED_Pin   , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_WH_RX].LED_Pin   , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_UP_TX].LED_Pin   , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_UP_RX].LED_Pin   , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_DN_TX].LED_Pin   , 1);
    LED_GPIO_Set(*LED_MODE_Str[LED_DN_RX].LED_Pin   , 1);
}


uint32_t LED_Tick = 0;
void LED_Level_Send(struct LED_MODE *p_Mode)
{
    switch(p_Mode->LED_Work_Mode)
    {
        case LED_OFF:
            p_Mode->LED_Work_Sta = 1;
            break;
        case LED_SLOW:
            if(0 == LED_Tick%8)
            {
                p_Mode->LED_Work_Sta = (p_Mode->LED_Work_Sta+1)%2;
            }
            break;
        case LED_NORMAL:
            if(0 == LED_Tick%4)
            {
                p_Mode->LED_Work_Sta = (p_Mode->LED_Work_Sta+1)%2;
            }
            break;
        case LED_FAST:
                p_Mode->LED_Work_Sta = (p_Mode->LED_Work_Sta+1)%2;
            break;
        case LED_ON:
            p_Mode->LED_Work_Sta = 0;
            break;
        default:
            break;
    }
}

uint32_t LED_All_TimeOut;
/* LED灯超时时间检查 */
void LED_TimeOut_Check(struct LED_MODE *p_Mode)
{
    if(-1 != p_Mode->LED_TimeOut)
    {
        p_Mode->LED_TimeOut--;
        if(-1 == p_Mode->LED_TimeOut)
        {
            p_Mode->LED_Work_Mode = LED_OFF;    // 超时时间到了熄灭
        }
    }
}

/*
 * Led_type:LED灯号
 * Work_Mode:灯工作模式
 *  */
uint8_t LED_All_Enable = 0;
void LED_Ctrl(uint8_t Led_type, uint8_t Work_Mode, uint32_t TimeOut)
{
    if(Led_type > LED_ALL)
        return ;
    if(LED_All_Enable)
    {
        Last_LED_MODE_Str[Led_type].LED_Work_Mode = Work_Mode;
        if(-1 != TimeOut)
        {
            Last_LED_MODE_Str[Led_type].LED_TimeOut = TimeOut/DELAY_TICK;
        }
    }
    else
    {
        LED_MODE_Str[Led_type].LED_Work_Mode = Work_Mode;
        if(-1 != TimeOut)
        {
            LED_MODE_Str[Led_type].LED_TimeOut = TimeOut/DELAY_TICK;
        }
    }
}

/*
 * Enable：使能，0：关闭全灯闪烁，非0：开启全灯闪烁
 * Work_Mode:灯工作模式(失能下无效)
 *  */
void LED_All_Ctrl(uint8_t Enable, uint8_t Work_Mode, uint32_t TimeOut)
{
    if(Enable)
    {
        LED_All_TimeOut = TimeOut/DELAY_TICK;
        if(0 == LED_All_Enable)
        {
            memcpy(Last_LED_MODE_Str, LED_MODE_Str, sizeof(LED_MODE_Str));
        }
        for (int i = 0; i < LED_ALL; ++i)
        {
            LED_MODE_Str[i].LED_Work_Sta  = 0;
            LED_MODE_Str[i].LED_Work_Mode = Work_Mode;
            if(0xFFFFFFFF != TimeOut)
            {
                LED_MODE_Str[i].LED_TimeOut = LED_All_TimeOut;
            }
        }
    }
    else
    {
        memcpy(LED_MODE_Str, Last_LED_MODE_Str, sizeof(LED_MODE_Str));
    }
    LED_All_Enable = Enable;
}


void LED_Ctrl_Cmd(int argc, char **argv)
{
    uint8_t Led_type = 0;
    uint8_t Work_Mode = 0;
    if(argc >= 3)
    {
        Led_type  = atoi(argv[1]);
        Work_Mode = atoi(argv[2]);
        LED_Ctrl(Led_type, Work_Mode, -1);
    }
}
#if OS_RTTHREAD
MSH_CMD_EXPORT(LED_Ctrl_Cmd, LED_Ctrl_Cmd);
#endif


void LED_Ctrl_Loop(void)
{
    for (int i = 0; i < LED_ALL; ++i)
    {
        LED_Level_Send(&LED_MODE_Str[i]);       // 灯闪实时状态计算
        LED_TimeOut_Check(&LED_MODE_Str[i]);    // 灯闪超时时间检测

        LED_GPIO_Set(*LED_MODE_Str[i].LED_Pin ,LED_MODE_Str[i].LED_Work_Sta);

    }
    if(0 != LED_All_TimeOut)
    {
        LED_All_TimeOut--;
        if(0 == LED_All_TimeOut)
        {
            LED_All_Ctrl(0, LED_OFF, -1);
        }
    }

    LED_Tick++;
}


void User_Led_Thread_Entry(void* p)
{
    LED_GPIO_Init();
    LED_Init();
//    extern void iwdg_entry(void);
    while(1)
    {
//        iwdg_entry();
        LED_Ctrl_Loop();
        rt_thread_mdelay(DELAY_TICK);
    }
}
