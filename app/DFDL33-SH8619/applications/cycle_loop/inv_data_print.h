/*
 * Copyright (c) 2006-2026, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef APPLICATIONS_CYCLE_LOOP_INV_DATA_PRINT_H_
#define APPLICATIONS_CYCLE_LOOP_INV_DATA_PRINT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 打印全部12个档案槽位及其对应的实时采集数据。 */
void Inv_Data_Print_All(void);

/* 按1～12档案编号打印单台逆变器实时数据，编号越界时打印参数错误。 */
void Inv_Data_Print_Archive(uint8_t archive_number);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATIONS_CYCLE_LOOP_INV_DATA_PRINT_H_ */
