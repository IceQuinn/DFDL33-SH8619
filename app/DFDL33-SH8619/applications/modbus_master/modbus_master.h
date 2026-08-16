/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-11     mutou       the first version
 */
#ifndef APPLICATIONS_MODBUS_MASTER_MODBUS_MASTER_H_
#define APPLICATIONS_MODBUS_MASTER_MODBUS_MASTER_H_

#include <rtthread.h>

#define MODBUS_SLAVE_ADDR_MIN            1
#define MODBUS_SLAVE_ADDR_MAX            247
#define MODBUS_RTU_ADU_MAX               256
#define MODBUS_READ_REG_MAX              125
#define MODBUS_READ_REQUEST_LEN          8

#define MODBUS_FUNC_READ_HOLDING         0x03
#define MODBUS_FUNC_READ_INPUT           0x04

#define MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS 0x02U /* 请求的寄存器地址或寄存器范围不被从站支持。 */

// 解帧结果只描述协议检查结果，发送错误和回复超时由调用者处理
typedef enum
{
    MODBUS_M_PARSE_OK = 0,              /* 响应报文校验通过，寄存器数据已经成功解析。 */
    MODBUS_M_PARSE_INVALID_ARGUMENT,    /* 从站地址、功能码、寄存器数量、指针或缓冲区容量参数无效。 */
    MODBUS_M_PARSE_CRC_ERROR,           /* 响应报文携带的CRC与本地计算结果不一致。 */
    MODBUS_M_PARSE_ADDRESS_ERROR,       /* 响应报文中的从站地址与本次请求地址不一致。 */
    MODBUS_M_PARSE_FUNCTION_ERROR,      /* 响应功能码既不是请求功能码，也不是对应的异常功能码。 */
    MODBUS_M_PARSE_LENGTH_ERROR,        /* 报文总长度、异常帧长度或寄存器字节数量不符合预期。 */
    MODBUS_M_PARSE_EXCEPTION            /* 从站返回Modbus异常响应，具体异常码写入exception_code。 */
}modbus_m_parse_result;

rt_uint16_t modbus_m_crc16(const rt_uint8_t *data, rt_uint16_t len);

rt_err_t modbus_m_read_request(rt_uint8_t slave_addr,
                               rt_uint8_t function_code,
                               rt_uint16_t start_addr,
                               rt_uint16_t register_count,
                               rt_uint8_t *frame,
                               rt_uint16_t frame_size,
                               rt_uint16_t *frame_len);

modbus_m_parse_result modbus_m_read_response(rt_uint8_t slave_addr,
                                             rt_uint8_t function_code,
                                             rt_uint16_t expected_register_count,
                                             const rt_uint8_t *frame,
                                             rt_uint16_t frame_len,
                                             rt_uint16_t *registers,
                                             rt_uint16_t register_capacity,
                                             rt_uint16_t *register_count,
                                             rt_uint8_t *exception_code);

#endif // APPLICATIONS_MODBUS_MASTER_MODBUS_MASTER_H_
