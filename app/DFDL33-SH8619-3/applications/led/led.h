/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-10-27     IP155       the first version
 */
#ifndef APPLICATIONS_SRC_USER_LED_H_
#define APPLICATIONS_SRC_USER_LED_H_

#include <stdint.h>

//灯
enum
{
    LED_RUN,        //运行状态灯     低电平有效
    LED_WH_TX,      //维护状态灯-发送状态        低电平有效
    LED_WH_RX,      //维护状态灯-接收状态        低电平有效
    LED_UP_TX,      //上行通信状态灯-发送状态      低电平有效
    LED_UP_RX,      //上行通信状态灯-接收状态      低电平有效
    LED_DN_TX,      //下行通信状态灯-发送状态      低电平有效
    LED_DN_RX,      //下行通信状态灯-接收状态      低电平有效

    LED_ALL,        // 所有灯
};


//灯闪烁状态
enum
{
    LED_OFF     ,   //不亮
    LED_SLOW    ,   //慢闪
    LED_NORMAL  ,   //正常闪
    LED_FAST    ,   //快闪
    LED_ON      ,   //常亮
};

/*
 * Led_type:LED灯号
 * Work_Mode:灯工作模式
 *  */
void LED_Ctrl(uint8_t Led_type, uint8_t Work_Mode, uint32_t TimeOut);
/*
 * Enable：使能，0：关闭全灯闪烁，非0：开启全灯闪烁
 * Work_Mode:灯工作模式(失能下无效)
 *  */
void LED_All_Ctrl(uint8_t Enable, uint8_t Work_Mode, uint32_t TimeOut);

void User_Led_Thread_Entry(void* p);

#endif /* APPLICATIONS_SRC_USER_LED_H_ */
