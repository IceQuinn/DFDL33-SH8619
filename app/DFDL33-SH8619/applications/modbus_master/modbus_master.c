/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-08-11     mutou       the first version
 */
#include "modbus_master.h"

// 计算Modbus RTU CRC16，组帧时先放低字节，再放高字节
rt_uint16_t modbus_m_crc16(const rt_uint8_t *data, rt_uint16_t len)
{
    rt_uint16_t crc = 0xFFFF;

    if((data == RT_NULL) && (len > 0))
    {
        return 0;
    }

    for(rt_uint16_t i = 0; i < len; ++i)
    {
        crc ^= data[i];

        for(rt_uint8_t bit = 0; bit < 8; ++bit)
        {
            if((crc & 0x0001) != 0)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

// 组03或04读寄存器请求帧，功能码由调用者传入
rt_err_t modbus_m_read_request(rt_uint8_t slave_addr,
                               rt_uint8_t function_code,
                               rt_uint16_t start_addr,
                               rt_uint16_t register_count,
                               rt_uint8_t *frame,
                               rt_uint16_t frame_size,
                               rt_uint16_t *frame_len)
{
    rt_uint16_t crc;

    if(frame_len != RT_NULL)
    {
        *frame_len = 0;
    }

    if((slave_addr < MODBUS_SLAVE_ADDR_MIN) || (slave_addr > MODBUS_SLAVE_ADDR_MAX) ||
       ((function_code != MODBUS_FUNC_READ_HOLDING) &&
        (function_code != MODBUS_FUNC_READ_INPUT)) ||
       (register_count == 0) || (register_count > MODBUS_READ_REG_MAX) ||
       (frame == RT_NULL) || (frame_len == RT_NULL) ||
       (frame_size < MODBUS_READ_REQUEST_LEN))
    {
        return -RT_EINVAL;
    }

    frame[0] = slave_addr;
    frame[1] = function_code;
    frame[2] = (rt_uint8_t)(start_addr >> 8);
    frame[3] = (rt_uint8_t)start_addr;
    frame[4] = (rt_uint8_t)(register_count >> 8);
    frame[5] = (rt_uint8_t)register_count;

    crc = modbus_m_crc16(frame, 6);
    frame[6] = (rt_uint8_t)crc;
    frame[7] = (rt_uint8_t)(crc >> 8);
    *frame_len = MODBUS_READ_REQUEST_LEN;

    return RT_EOK;
}

// 解03或04回复帧，调用者提供寄存器输出缓冲区，函数内部不保存任何数据
modbus_m_parse_result modbus_m_read_response(rt_uint8_t slave_addr,
                                             rt_uint8_t function_code,
                                             rt_uint16_t expected_register_count,
                                             const rt_uint8_t *frame,
                                             rt_uint16_t frame_len,
                                             rt_uint16_t *registers,
                                             rt_uint16_t register_capacity,
                                             rt_uint16_t *register_count,
                                             rt_uint8_t *exception_code)
{
    rt_uint16_t received_crc;
    rt_uint16_t calculated_crc;
    rt_uint8_t byte_count;

    if(register_count != RT_NULL)
    {
        *register_count = 0;
    }

    if(exception_code != RT_NULL)
    {
        *exception_code = 0;
    }

    if((slave_addr < MODBUS_SLAVE_ADDR_MIN) || (slave_addr > MODBUS_SLAVE_ADDR_MAX) ||
       ((function_code != MODBUS_FUNC_READ_HOLDING) &&
        (function_code != MODBUS_FUNC_READ_INPUT)) ||
       (expected_register_count == 0) ||
       (expected_register_count > MODBUS_READ_REG_MAX) ||
       (frame == RT_NULL) || (registers == RT_NULL) ||
       (register_count == RT_NULL) || (exception_code == RT_NULL) ||
       (register_capacity < expected_register_count))
    {
        return MODBUS_M_PARSE_INVALID_ARGUMENT;
    }

    if(frame_len < 5)
    {
        return MODBUS_M_PARSE_LENGTH_ERROR;
    }

    received_crc = frame[frame_len - 2] | ((rt_uint16_t)frame[frame_len - 1] << 8);
    calculated_crc = modbus_m_crc16(frame, frame_len - 2);
    if(received_crc != calculated_crc)
    {
        return MODBUS_M_PARSE_CRC_ERROR;
    }

    if(frame[0] != slave_addr)
    {
        return MODBUS_M_PARSE_ADDRESS_ERROR;
    }

    if(frame[1] == (function_code | 0x80))
    {
        if(frame_len != 5)
        {
            return MODBUS_M_PARSE_LENGTH_ERROR;
        }

        *exception_code = frame[2];
        return MODBUS_M_PARSE_EXCEPTION;
    }

    if(frame[1] != function_code)
    {
        return MODBUS_M_PARSE_FUNCTION_ERROR;
    }

    byte_count = frame[2];
    if((byte_count != expected_register_count * 2) ||
       (frame_len != (rt_uint16_t)(byte_count + 5)))
    {
        return MODBUS_M_PARSE_LENGTH_ERROR;
    }

    for(rt_uint16_t i = 0; i < expected_register_count; ++i)
    {
        registers[i] = ((rt_uint16_t)frame[3 + i * 2] << 8) |
                       frame[4 + i * 2];
    }

    *register_count = expected_register_count;
    return MODBUS_M_PARSE_OK;
}
