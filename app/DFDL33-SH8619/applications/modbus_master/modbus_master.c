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

/* 将解析结果转换为便于通信日志直接打印的简短英文说明。 */
const char *modbus_m_parse_result_text(modbus_m_parse_result result)
{
    /* 每种解析结果返回固定文本，未知值统一返回unknown result。 */
    switch(result) {
    case MODBUS_M_PARSE_OK:
        return "reply OK";

    case MODBUS_M_PARSE_INVALID_ARGUMENT:
        return "invalid argument";

    case MODBUS_M_PARSE_CRC_ERROR:
        return "CRC error";

    case MODBUS_M_PARSE_ADDRESS_ERROR:
        return "wrong slave address";

    case MODBUS_M_PARSE_FUNCTION_ERROR:
        return "wrong function code";

    case MODBUS_M_PARSE_LENGTH_ERROR:
        return "wrong frame length";

    case MODBUS_M_PARSE_EXCEPTION:
        return "device exception";

    default:
        return "unknown result";
    }
}

/* 计算Modbus RTU CRC16，组帧时先放CRC低字节，再放CRC高字节。 */
uint16_t modbus_m_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t index;
    uint8_t bit;

    /* 长度大于0时数据指针不能为空，空数据且长度为0允许计算。 */
    if((data == RT_NULL) && (len > 0U)) {
        return 0U;
    }

    /* 每个输入字节依次参与CRC计算。 */
    for(index = 0U; index < len; ++index) {
        crc ^= data[index];

        /* 一个输入字节需要连续处理8个二进制位。 */
        for(bit = 0U; bit < 8U; ++bit) {
            /* CRC最低位为1时，右移后需要与Modbus多项式0xA001异或。 */
            if((crc & 0x0001U) != 0U) {
                crc = (crc >> 1U) ^ 0xA001U;
            }
            /* CRC最低位为0时只执行右移。 */
            else {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

/* 根据从站地址、功能码、起始地址和寄存器数量组成03或04功能码请求帧。 */
rt_err_t modbus_m_read_request(uint8_t slave_addr,
                               uint8_t function_code,
                               uint16_t start_addr,
                               uint16_t register_count,
                               uint8_t *frame,
                               uint16_t frame_size,
                               uint16_t *frame_len)
{
    uint16_t crc;

    /* 输出长度指针有效时先清零，确保组帧失败不会保留旧长度。 */
    if(frame_len != RT_NULL) {
        *frame_len = 0U;
    }

    /* 任意输入参数越界、输出指针为空或输出缓冲区不足时拒绝组帧。 */
    if((slave_addr < MODBUS_SLAVE_ADDR_MIN) || (slave_addr > MODBUS_SLAVE_ADDR_MAX) ||
       ((function_code != MODBUS_FUNC_READ_HOLDING) && (function_code != MODBUS_FUNC_READ_INPUT)) ||
       (register_count == 0U) || (register_count > MODBUS_READ_REG_MAX) ||
       (((uint32_t)start_addr + register_count) > 0x10000U) ||
       (frame == RT_NULL) || (frame_len == RT_NULL) || (frame_size < MODBUS_READ_REQUEST_LEN)) {
        return -RT_EINVAL;
    }

    frame[0] = slave_addr;
    frame[1] = function_code;
    frame[2] = (uint8_t)(start_addr >> 8U);
    frame[3] = (uint8_t)start_addr;
    frame[4] = (uint8_t)(register_count >> 8U);
    frame[5] = (uint8_t)register_count;

    crc = modbus_m_crc16(frame, 6U);
    frame[6] = (uint8_t)crc;
    frame[7] = (uint8_t)(crc >> 8U);
    *frame_len = MODBUS_READ_REQUEST_LEN;
    return RT_EOK;
}

/* 解析03或04功能码响应，解析结果通过返回值区分，寄存器数据写入调用方缓冲区。 */
modbus_m_parse_result modbus_m_read_response(uint8_t slave_addr,
                                             uint8_t function_code,
                                             uint16_t expected_register_count,
                                             const uint8_t *frame,
                                             uint16_t frame_len,
                                             uint16_t *registers,
                                             uint16_t register_capacity,
                                             uint16_t *register_count,
                                             uint8_t *exception_code)
{
    uint16_t received_crc;
    uint16_t calculated_crc;
    uint16_t index;
    uint8_t byte_count;

    /* 输出寄存器数量指针有效时先清零，避免解析失败后误用旧数量。 */
    if(register_count != RT_NULL) {
        *register_count = 0U;
    }

    /* 输出异常码指针有效时先清零，正常响应不会留下旧异常码。 */
    if(exception_code != RT_NULL) {
        *exception_code = 0U;
    }

    /* 请求参数、报文指针、输出指针或寄存器缓冲区容量无效时停止解析。 */
    if((slave_addr < MODBUS_SLAVE_ADDR_MIN) || (slave_addr > MODBUS_SLAVE_ADDR_MAX) ||
       ((function_code != MODBUS_FUNC_READ_HOLDING) && (function_code != MODBUS_FUNC_READ_INPUT)) ||
       (expected_register_count == 0U) || (expected_register_count > MODBUS_READ_REG_MAX) ||
       (frame == RT_NULL) || (registers == RT_NULL) ||
       (register_count == RT_NULL) || (exception_code == RT_NULL) ||
       (register_capacity < expected_register_count)) {
        return MODBUS_M_PARSE_INVALID_ARGUMENT;
    }

    /* Modbus异常响应最短为5字节，长度不足时不能继续读取CRC和功能码。 */
    if(frame_len < 5U) {
        return MODBUS_M_PARSE_LENGTH_ERROR;
    }

    received_crc = frame[frame_len - 2U] | ((uint16_t)frame[frame_len - 1U] << 8U);
    calculated_crc = modbus_m_crc16(frame, frame_len - 2U);

    /* 接收CRC与本地计算结果不一致时，报文内容不可信。 */
    if(received_crc != calculated_crc) {
        return MODBUS_M_PARSE_CRC_ERROR;
    }

    /* 响应从站地址必须与本次请求地址相同。 */
    if(frame[0] != slave_addr) {
        return MODBUS_M_PARSE_ADDRESS_ERROR;
    }

    /* 功能码最高位置1表示从站返回了Modbus异常响应。 */
    if(frame[1] == (function_code | 0x80U)) {
        /* 标准Modbus异常响应固定为地址、异常功能码、异常码和2字节CRC。 */
        if(frame_len != 5U) {
            return MODBUS_M_PARSE_LENGTH_ERROR;
        }

        *exception_code = frame[2];
        return MODBUS_M_PARSE_EXCEPTION;
    }

    /* 非异常响应的功能码必须与请求功能码完全一致。 */
    if(frame[1] != function_code) {
        return MODBUS_M_PARSE_FUNCTION_ERROR;
    }

    byte_count = frame[2];

    /* 字节数必须等于寄存器数量的两倍，报文总长度还必须包含地址、功能码、字节数和CRC。 */
    if((byte_count != expected_register_count * 2U) || (frame_len != (uint16_t)(byte_count + 5U))) {
        return MODBUS_M_PARSE_LENGTH_ERROR;
    }

    /* Modbus寄存器按高字节在前的顺序转换为16位数据。 */
    for(index = 0U; index < expected_register_count; ++index) {
        registers[index] = ((uint16_t)frame[3U + index * 2U] << 8U) | frame[4U + index * 2U];
    }

    *register_count = expected_register_count;
    return MODBUS_M_PARSE_OK;
}
