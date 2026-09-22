/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-01-12     IP155       the first version
 */
#ifndef DRIVERS_USER_DRV_AB_CHECK_H_
#define DRIVERS_USER_DRV_AB_CHECK_H_

#include <stdint.h>

typedef struct
{
    uint16_t ver;       // 版本号
    uint16_t len;       // 配置长度，从ugain_a开始到结构体结束
    uint16_t crc16b;    // 校验和（从第一个参数开始计算，len长度）
}rcd_head;

int32_t AB_check(int32_t (*read)(uint32_t, void *, uint32_t),
            int32_t  (*write)(uint32_t, void *, uint32_t),
            uint32_t a_addr,
            uint32_t b_addr,
            void     *buf,
            uint32_t len,
            char     *desc);

/* 通用AB区接口 */
int32_t AB_save(int32_t (*write)(uint32_t, void *, uint32_t),
            uint32_t a_addr,
            uint32_t b_addr,
            void     *buf,
            uint32_t ver,
            uint32_t len,
            char     *desc);

#endif /* DRIVERS_USER_DRV_AB_CHECK_H_ */
