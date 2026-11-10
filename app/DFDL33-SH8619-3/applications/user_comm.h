/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-07     mutou       the first version
 */
#ifndef __USER_COMM_H__
#define __USER_COMM_H__

//数据类型
enum data_type
{
    TYPE_I8,        TYPE_U8,
    TYPE_I16,       TYPE_U16,
    TYPE_I32,       TYPE_U32,
    TYPE_FLOAT32,   TYPE_FLOAT64,
    TYPE_ASCII,
    TYPE_BCD,
    TYPE_BCD_TIME,
    TYPE_BIT_FIELD,
};


#endif
