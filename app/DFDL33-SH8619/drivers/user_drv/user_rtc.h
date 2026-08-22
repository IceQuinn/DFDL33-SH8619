/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-12-18     mutou       the first version
 */
#ifndef DRIVERS_USER_DRV_INC_USER_RTC_H_
#define DRIVERS_USER_DRV_INC_USER_RTC_H_

#include <rtthread.h>
#include <rtdevice.h>
#include <stdint.h>
#include <time.h>


int rtc_reinit(void);
void rtc_init(void);
void show_rtc_time(void);

uint8_t  get_month(void);
uint16_t get_rtc_ms(void);
char *get_char_time(void);

uint16_t set_year(uint16_t year);
uint16_t set_month(uint16_t month);
uint16_t set_day(uint16_t day);
uint16_t set_hour(uint16_t hour);
uint16_t set_min(uint16_t min);
uint16_t set_sec(uint16_t sec);

void set_timestamp(time_t sec);

struct tm get_local_time_t(void);


#endif /* DRIVERS_USER_DRV_INC_USER_RTC_H_ */
