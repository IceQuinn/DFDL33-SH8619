#ifndef __VOLTAGE_ACQ_H__
#define __VOLTAGE_ACQ_H__

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "at32f403a_407.h"
#include <math.h>

#define VOL_IDLE_MS          500

extern float    g_voltage_rms;

int  voltage_acq_init(void);
void start_voltage_sampling(void);
float calculate_rms(uint16_t *buf, uint8_t len);
void voltage_acq_thread_entry(void *param);

#endif

