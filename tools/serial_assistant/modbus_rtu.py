"""Modbus RTU master request parsing helpers.

The CRC bytes in an RTU frame are transmitted low byte first.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


FUNCTION_NAMES = {
    0x01: "读线圈",
    0x02: "读离散输入",
    0x03: "读保持寄存器",
    0x04: "读输入寄存器",
    0x05: "写单个线圈",
    0x06: "写单个保持寄存器",
    0x08: "诊断",
    0x0F: "写多个线圈",
    0x10: "写多个保持寄存器",
    0x16: "屏蔽写寄存器",
    0x17: "读/写多个保持寄存器",
}


def crc16(data: bytes) -> int:
    """Return Modbus CRC-16 as an integer (polynomial 0xA001)."""
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def parse_hex(text: str) -> bytes:
    """Parse user supplied hex, accepting spaces, commas and 0x prefixes."""
    cleaned = text.replace("0x", "").replace("0X", "")
    for separator in (",", "-", ":", "\n", "\r", "\t"):
        cleaned = cleaned.replace(separator, " ")
    parts = cleaned.split()
    if len(parts) == 1 and len(parts[0]) > 2:
        token = parts[0]
        if len(token) % 2:
            raise ValueError("连续十六进制字符数必须为偶数")
        parts = [token[index:index + 2] for index in range(0, len(token), 2)]
    try:
        values = bytes(int(part, 16) for part in parts)
    except ValueError as exc:
        raise ValueError("请输入有效的十六进制字节，例如：01 03 00 00 00 02") from exc
    if any(len(part) > 2 for part in parts):
        raise ValueError("每个十六进制字节不能超过两位")
    return values


@dataclass(frozen=True)
class ParseResult:
    valid: bool
    device_address: Optional[int]
    function_code: Optional[int]
    operation: str
    register_address: Optional[int]
    register_count: Optional[int]
    crc_ok: bool
    received_crc: Optional[int]
    calculated_crc: Optional[int]
    detail: str = ""
    write_values: tuple[int, ...] = ()
    read_address: Optional[int] = None
    read_count: Optional[int] = None

    @property
    def function_name(self) -> str:
        if self.function_code is None:
            return "未知"
        return FUNCTION_NAMES.get(self.function_code, "未知功能码")


def _u16(frame: bytes, offset: int) -> int:
    return (frame[offset] << 8) | frame[offset + 1]


def parse_master_request(frame: bytes) -> ParseResult:
    """Parse one Modbus RTU master request, including its trailing CRC."""
    if len(frame) < 4:
        return ParseResult(False, frame[0] if frame else None, None, "未知", None,
                           None, False, None, None, "报文过短，至少需要4字节")

    address, function = frame[0], frame[1]
    received_crc = frame[-2] | (frame[-1] << 8)
    calculated_crc = crc16(frame[:-2])
    crc_ok = received_crc == calculated_crc
    operation = "未知"
    reg_address = None
    reg_count = None
    values: tuple[int, ...] = ()
    read_address = None
    read_count = None
    details: list[str] = []
    minimum = 8
    supported = function in FUNCTION_NAMES
    semantic_ok = True

    if address > 247:
        details.append(f"设备地址 {address} 超出 Modbus 范围 0~247")
        semantic_ok = False

    try:
        if function in (0x01, 0x02, 0x03, 0x04):
            operation = "读"
            reg_address, reg_count = _u16(frame, 2), _u16(frame, 4)
            maximum = 2000 if function in (0x01, 0x02) else 125
            if not 1 <= reg_count <= maximum:
                details.append(f"读取数量应为 1~{maximum}，实际为 {reg_count}")
                semantic_ok = False
        elif function == 0x05:
            operation = "写"
            reg_address, reg_count = _u16(frame, 2), 1
            raw = _u16(frame, 4)
            values = (raw,)
            details.append("线圈值：" + ("ON" if raw == 0xFF00 else "OFF" if raw == 0 else f"非法值 0x{raw:04X}"))
            if raw not in (0x0000, 0xFF00):
                semantic_ok = False
        elif function == 0x06:
            operation = "写"
            reg_address, reg_count = _u16(frame, 2), 1
            values = (_u16(frame, 4),)
            details.append(f"写入值：0x{values[0]:04X} ({values[0]})")
        elif function in (0x0F, 0x10):
            operation = "写"
            reg_address, reg_count = _u16(frame, 2), _u16(frame, 4)
            byte_count = frame[6]
            minimum = 9 + byte_count
            payload = frame[7:7 + byte_count]
            if function == 0x10:
                values = tuple(_u16(payload, i) for i in range(0, len(payload) - 1, 2))
                details.append("写入值：" + ", ".join(f"0x{x:04X}" for x in values))
            else:
                details.append(f"线圈数据：{hex_bytes(payload)}")
            expected = reg_count * 2 if function == 0x10 else (reg_count + 7) // 8
            if byte_count != expected:
                details.append(f"字节数异常（声明 {byte_count}，应为 {expected}）")
                semantic_ok = False
            maximum = 123 if function == 0x10 else 1968
            if not 1 <= reg_count <= maximum:
                details.append(f"写入数量应为 1~{maximum}，实际为 {reg_count}")
                semantic_ok = False
        elif function == 0x16:
            operation = "写"
            reg_address, reg_count = _u16(frame, 2), 1
            minimum = 10
            details.append(f"AND掩码：0x{_u16(frame, 4):04X}，OR掩码：0x{_u16(frame, 6):04X}")
        elif function == 0x17:
            operation = "读/写"
            read_address, read_count = _u16(frame, 2), _u16(frame, 4)
            reg_address, reg_count = _u16(frame, 6), _u16(frame, 8)
            byte_count = frame[10]
            minimum = 13 + byte_count
            payload = frame[11:11 + byte_count]
            values = tuple(_u16(payload, i) for i in range(0, len(payload) - 1, 2))
            details.append("写入值：" + ", ".join(f"0x{x:04X}" for x in values))
            if byte_count != reg_count * 2:
                details.append(f"写入字节数异常（声明 {byte_count}，应为 {reg_count * 2}）")
                semantic_ok = False
            if not 1 <= read_count <= 125:
                details.append(f"读取数量应为 1~125，实际为 {read_count}")
                semantic_ok = False
            if not 1 <= reg_count <= 121:
                details.append(f"写入数量应为 1~121，实际为 {reg_count}")
                semantic_ok = False
        elif function == 0x08:
            operation = "诊断"
            reg_address, reg_count = _u16(frame, 2), 1
            details.append(f"子功能码：0x{reg_address:04X}，数据：0x{_u16(frame, 4):04X}")
        else:
            minimum = 4
            details.append(f"暂不支持解析功能码 0x{function:02X} 的数据域")
    except IndexError:
        details.append("数据域不完整")
        semantic_ok = False

    length_ok = len(frame) >= minimum
    if len(frame) != minimum and function in FUNCTION_NAMES:
        suffix = f"报文长度 {len(frame)} 字节，应为 {minimum} 字节"
        details.append(suffix)
        length_ok = False
    return ParseResult(length_ok and semantic_ok and supported, address, function, operation, reg_address,
                       reg_count, crc_ok, received_crc, calculated_crc,
                       "；".join(details), values, read_address, read_count)
