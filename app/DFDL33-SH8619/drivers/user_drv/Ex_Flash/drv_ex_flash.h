/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-09-07     mutou       the first version
 */
#ifndef DRIVERS_USER_DRV_EX_FLASH_DRV_EX_FLASH_H_
#define DRIVERS_USER_DRV_EX_FLASH_DRV_EX_FLASH_H_

#include "drv_common.h"

int32_t flash_init(void);

int32_t flash_write(uint32_t flashaddr, void* data, uint32_t len);

int32_t flash_read(uint32_t flashaddr, void* data, uint32_t len);

#endif /* DRIVERS_USER_DRV_EX_FLASH_DRV_EX_FLASH_H_ */
