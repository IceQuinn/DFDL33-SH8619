#ifndef __HJ02C_H__
#define __HJ02C_H__

#include <rtthread.h>
#include <stdint.h>


rt_err_t hj02c_send_cmd_and_get_resp(const char *cmd, char *resp,
                                         uint16_t max_len, uint16_t *len);
/* ========== 配置指令 ========== */
rt_err_t hj02c_set_name(const char *name);
rt_err_t hj02c_set_adv_gap(uint16_t gap_ms);
rt_err_t hj02c_set_adv(uint8_t on);
rt_err_t hj02c_set_con_min_gap(uint16_t ms);
rt_err_t hj02c_set_con_max_gap(uint16_t ms);
rt_err_t hj02c_set_tx_power(int8_t dbm);
rt_err_t hj02c_set_high_speed(uint8_t on);
rt_err_t hj02c_reset(void);


rt_err_t hj02c_basic_init(const char *device_name);

void hj02c_rx_thread_entry(void *parameter);

#endif

