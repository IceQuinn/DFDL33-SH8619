#ifndef __VERSION__H
#define __VERSION__H

#include <stdint.h>

struct Project_Version_Str
{
    uint16_t major_version_number;	//主版本
    uint16_t minor_version_number;	//次版本
    uint16_t revision_number;		//修订版本
    uint16_t build_number;			//构建版本
};

struct Project_Date_Time_Str
{
    uint16_t hardware_year;     	//工程编译年份
    uint16_t hardware_mon;      	//工程编译月份
    uint16_t hardware_day;      	//工程编译日份
	uint16_t hardware_hour;			//工程编译小时
	uint16_t hardware_minute;		//工程编译分钟
	uint16_t hardware_second;		//工程编译秒钟
};

void show_ctu_msg(void);


#endif

