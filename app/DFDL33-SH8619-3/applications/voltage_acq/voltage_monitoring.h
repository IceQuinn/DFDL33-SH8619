#ifndef __VOLTAGE_MONITORING_H__
#define __VOLTAGE_MONITORING_H__

#include <stdint.h>


typedef struct st_over_voltage
{
    uint16_t *u_a;
//    uint16_t *u_b;
//    uint16_t *u_c;

    uint16_t *u_oversetting;     //定值
    uint32_t *t_setting;        //延时定值

    uint8_t  *fault_status;     //故障状态
    uint16_t event;             //触发事件
}over_voltage;

int over_voltage_check(over_voltage *p_over_voltage);




#endif
