"""DL/T 645-2007 frame, stream and configurable data-area codecs."""

from __future__ import annotations

import json
import re
import struct
from dataclasses import dataclass, field
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any, Iterable, Mapping, Optional


CONTROL_NAMES = {
    0x08: "广播校时",
    0x11: "读数据",
    0x12: "读后续数据",
    0x13: "读通信地址",
    0x14: "写数据",
    0x15: "写通信地址",
    0x16: "冻结命令",
    0x17: "更改通信速率",
    0x18: "修改密码",
    0x19: "最大需量清零",
    0x1A: "电表清零",
    0x1B: "事件清零",
}

ERROR_BITS = {
    0: "其他错误",
    1: "无请求数据",
    2: "密码错误/未授权",
    3: "通信速率不能更改",
    4: "年时区数超",
    5: "日时段数超",
    6: "费率数超",
}

DESCRIPTOR_DATA_TYPES = {
    0: "int_8", 1: "uint_8", 2: "int_16", 3: "uint_16", 4: "int_32", 5: "uint_32",
    6: "float_32", 7: "float_64", 8: "ASCII", 9: "BCD", 10: "BCD_TIME",
}
DESCRIPTOR_BYTE_ORDERS = {0: "ABCD", 1: "CDAB", 2: "BADC", 3: "DCBA"}


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def checksum(data: bytes) -> int:
    return sum(data) & 0xFF


def add_33(data: bytes) -> bytes:
    return bytes((value + 0x33) & 0xFF for value in data)


def subtract_33(data: bytes) -> bytes:
    return bytes((value - 0x33) & 0xFF for value in data)


def normalize_address(address: str) -> str:
    value = re.sub(r"[\s-]", "", address).upper()
    if value == "A" * 12:
        return value
    if not re.fullmatch(r"\d{12}", value):
        raise ValueError("645通信地址必须是12位十进制数字，广播地址可填写12个A")
    return value


def encode_address(address: str) -> bytes:
    value = normalize_address(address)
    return bytes.fromhex(value)[::-1]


def decode_address(raw: bytes) -> str:
    if len(raw) != 6:
        raise ValueError("645通信地址必须是6字节")
    return raw[::-1].hex().upper()


def normalize_di(data_identifier: str) -> str:
    value = re.sub(r"[\s-]", "", data_identifier).upper()
    if not re.fullmatch(r"[0-9A-F]{8}", value):
        raise ValueError("数据标识必须是8位十六进制字符")
    return value


def encode_di(data_identifier: str) -> bytes:
    return bytes.fromhex(normalize_di(data_identifier))[::-1]


def decode_di(raw: bytes) -> str:
    if len(raw) != 4:
        raise ValueError("DL/T 645-2007数据标识必须是4字节")
    return raw[::-1].hex().upper()


def build_frame(address: str, control: int, clear_data: bytes = b"", preamble: int = 0) -> bytes:
    if not 0 <= control <= 0xFF:
        raise ValueError("控制码必须为一个字节")
    if not 0 <= preamble <= 16:
        raise ValueError("前导FE数量必须为0~16")
    if len(clear_data) > 255:
        raise ValueError("数据域不能超过255字节")
    body = bytes((0x68,)) + encode_address(address) + bytes((0x68, control, len(clear_data))) + add_33(clear_data)
    return b"\xFE" * preamble + body + bytes((checksum(body), 0x16))


def build_read_address(preamble: int = 4) -> bytes:
    return build_frame("AAAAAAAAAAAA", 0x13, preamble=preamble)


def build_read_data(
    address: str,
    data_identifier: str,
    preamble: int = 0,
    follow_up: bool = False,
    sequence: int = 0,
) -> bytes:
    clear_data = encode_di(data_identifier)
    if follow_up:
        if not 0 <= sequence <= 0xFF:
            raise ValueError("后续帧序号必须为0~255")
        clear_data += bytes((sequence,))
    return build_frame(address, 0x12 if follow_up else 0x11, clear_data, preamble)


def _fixed_hex(value: str, byte_length: int, label: str) -> bytes:
    cleaned = re.sub(r"[\s-]", "", value).upper()
    if not re.fullmatch(rf"[0-9A-F]{{{byte_length * 2}}}", cleaned):
        raise ValueError(f"{label}必须是{byte_length * 2}位十六进制字符")
    return bytes.fromhex(cleaned)


def build_write_data(
    address: str,
    data_identifier: str,
    payload: bytes,
    password_permission: str = "00",
    password: str = "000000",
    operator_code: str = "00000000",
    preamble: int = 0,
    include_security: bool = True,
) -> bytes:
    clear_data = encode_di(data_identifier)
    if include_security:
        clear_data += _fixed_hex(password_permission, 1, "密码权限")
        clear_data += _fixed_hex(password, 3, "密码")
        clear_data += _fixed_hex(operator_code, 4, "操作者代码")
    clear_data += payload
    return build_frame(address, 0x14, clear_data, preamble)


@dataclass(frozen=True)
class DLT645Result:
    valid: bool
    raw: bytes
    preamble_count: int
    address: Optional[str]
    control: Optional[int]
    operation: str
    direction: str
    abnormal: bool
    follow_up: bool
    checksum_ok: bool
    received_checksum: Optional[int]
    calculated_checksum: Optional[int]
    clear_data: bytes = b""
    data_identifier: Optional[str] = None
    payload: bytes = b""
    error_code: Optional[int] = None
    error_text: str = ""
    detail: str = ""
    decoded_values: tuple[tuple[str, str, str], ...] = ()
    frame_sequence: Optional[int] = None


def parse_frame(frame: bytes, registry: Optional["DataIdentifierRegistry"] = None) -> DLT645Result:
    raw = bytes(frame)
    preamble = 0
    while preamble < len(raw) and raw[preamble] == 0xFE:
        preamble += 1
    body = raw[preamble:]
    if len(body) < 12:
        return DLT645Result(False, raw, preamble, None, None, "未知", "未知", False, False,
                            False, None, None, detail="报文过短")
    address = decode_address(body[1:7]) if body[0] == 0x68 else None
    control = body[8] if len(body) > 8 else None
    direction = "从机→主机" if control is not None and control & 0x80 else "主机→从机"
    abnormal = bool(control is not None and control & 0x40)
    follow_up = bool(control is not None and control & 0x20)
    function = control & 0x1F if control is not None else -1
    operation = CONTROL_NAMES.get(function, f"未知功能 0x{function:02X}")
    details: list[str] = []
    structure_ok = True
    if address and not re.fullmatch(r"(?:\d{12}|A{12})", address):
        details.append(f"通信地址不是有效BCD：{address}")
        structure_ok = False
    if body[0] != 0x68:
        details.append("缺少起始符68")
        structure_ok = False
    if body[7] != 0x68:
        details.append("第二个起始符不是68")
        structure_ok = False
    length = body[9]
    expected = 12 + length
    if len(body) != expected:
        details.append(f"报文长度{len(body)}字节，应为{expected}字节")
        structure_ok = False
    data_end = min(10 + length, max(10, len(body) - 2))
    wire_data = body[10:data_end]
    clear_data = subtract_33(wire_data)
    received = body[10 + length] if len(body) > 10 + length else None
    calculated = checksum(body[:10 + length]) if len(body) >= 10 + length else None
    checksum_ok = received is not None and calculated == received
    if not checksum_ok:
        details.append("校验和错误")
    if len(body) <= 11 + length or body[11 + length] != 0x16:
        details.append("缺少结束符16")
        structure_ok = False

    data_identifier = None
    payload = b""
    error_code = None
    error_text = ""
    decoded: tuple[tuple[str, str, str], ...] = ()
    frame_sequence = None
    if abnormal:
        if clear_data:
            error_code = clear_data[0]
            messages = [text for bit, text in ERROR_BITS.items() if error_code & (1 << bit)]
            error_text = "、".join(messages) or f"未知错误 0x{error_code:02X}"
            details.append(error_text)
        else:
            details.append("异常响应缺少错误码")
            structure_ok = False
    elif function in (0x11, 0x12, 0x14) and len(clear_data) >= 4:
        data_identifier = decode_di(clear_data[:4])
        if function == 0x14 and direction == "主机→从机":
            # Standard write requests carry 4 password bytes and 4 operator bytes.
            include_security = bool(registry.defaults.get("write_include_security", True)) if registry else True
            if include_security and len(clear_data) < 12:
                details.append("标准写数据请求缺少密码或操作者代码")
                structure_ok = False
                payload = b""
            else:
                payload = clear_data[12:] if include_security else clear_data[4:]
        else:
            payload = clear_data[4:]
            if function == 0x12 or follow_up:
                if payload:
                    frame_sequence = payload[-1]
                    payload = payload[:-1]
                else:
                    details.append("后续数据帧缺少帧序号")
                    structure_ok = False
        if registry and data_identifier and payload and not follow_up and function != 0x12:
            try:
                section = "write_request" if function == 0x14 and direction == "主机→从机" else "read_response"
                decoded = tuple(registry.decode(data_identifier, payload, section))
            except ValueError as exc:
                details.append(str(exc))
                structure_ok = False
    elif function in (0x11, 0x12) and clear_data:
        details.append("数据域不足4字节，无法取得数据标识")
        structure_ok = False

    return DLT645Result(
        structure_ok and checksum_ok, raw, preamble, address, control, operation, direction,
        abnormal, follow_up, checksum_ok, received, calculated, clear_data, data_identifier,
        payload, error_code, error_text, "；".join(details), decoded, frame_sequence,
    )


def looks_like_frame(data: bytes) -> bool:
    offset = 0
    while offset < len(data) and data[offset] == 0xFE:
        offset += 1
    return len(data) >= offset + 8 and data[offset] == 0x68 and data[offset + 7] == 0x68


class DLT645StreamParser:
    """Extract complete 645 frames from fragmented, concatenated or noisy bytes."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.unparsed = bytearray()

    def _discard(self, count: int) -> None:
        """Move bytes that cannot belong to a valid frame into the visible reject buffer."""
        if count > 0:
            self.unparsed.extend(self.buffer[:count])
            del self.buffer[:count]

    def take_unparsed(self, finalize: bool = False) -> bytes:
        """Return rejected bytes; optionally treat an idle-ended partial frame as rejected."""
        if finalize and self.buffer:
            self.unparsed.extend(self.buffer)
            self.buffer.clear()
        data = bytes(self.unparsed)
        self.unparsed.clear()
        return data

    def feed(self, data: bytes) -> list[bytes]:
        self.buffer.extend(data)
        frames: list[bytes] = []
        while self.buffer:
            start = self.buffer.find(0x68)
            if start < 0:
                # Retain only a trailing run of possible preamble bytes.
                suffix = 0
                for value in reversed(self.buffer):
                    if value != 0xFE or suffix >= 16:
                        break
                    suffix += 1
                self._discard(len(self.buffer) - suffix)
                break
            preamble_start = start
            while preamble_start > 0 and self.buffer[preamble_start - 1] == 0xFE:
                preamble_start -= 1
            if start + 10 > len(self.buffer):
                if preamble_start:
                    self._discard(preamble_start)
                break
            if self.buffer[start + 7] != 0x68:
                self._discard(start + 1)
                continue
            total_body = 12 + self.buffer[start + 9]
            end = start + total_body
            if end > len(self.buffer):
                if preamble_start:
                    self._discard(preamble_start)
                break
            candidate = bytes(self.buffer[preamble_start:end])
            if self.buffer[end - 1] != 0x16:
                self._discard(start + 1)
                continue
            if preamble_start:
                self._discard(preamble_start)
                end -= preamble_start
                candidate = bytes(self.buffer[:end])
            frames.append(candidate)
            del self.buffer[:end]
        return frames


def _decimal(value: Any, label: str) -> Decimal:
    try:
        return Decimal(str(value))
    except InvalidOperation as exc:
        raise ValueError(f"{label}不是有效数字：{value}") from exc


def _byte_order(field: Mapping[str, Any]) -> str:
    order = str(field.get("byte_order", "little")).lower()
    if order not in ("little", "big"):
        raise ValueError(f"字段{field.get('name', '')}的byte_order必须是little或big")
    return order


def _apply_sign_bit(raw: bytearray, field: Mapping[str, Any]) -> bool:
    sign = field.get("sign_bit")
    if not sign:
        return False
    index = int(sign.get("byte", len(raw) - 1))
    mask = int(str(sign.get("mask", "80")), 16)
    if not 0 <= index < len(raw):
        raise ValueError(f"字段{field.get('name', '')}的sign_bit.byte越界")
    negative = bool(raw[index] & mask)
    raw[index] &= ~mask & 0xFF
    return negative


def decode_field(raw_value: bytes, definition: Mapping[str, Any]) -> Any:
    kind = str(definition.get("type", "hex")).lower()
    order = _byte_order(definition)
    logical = raw_value[::-1] if order == "little" else raw_value
    if kind == "type_descriptor":
        descriptor = int.from_bytes(raw_value, order, signed=False)
        data_type = DESCRIPTOR_DATA_TYPES.get(descriptor & 0x0F, f"保留({descriptor & 0x0F})")
        byte_order = DESCRIPTOR_BYTE_ORDERS.get((descriptor >> 4) & 0x0F, f"保留({(descriptor >> 4) & 0x0F})")
        decimals = (descriptor >> 8) & 0x0F
        return f"{data_type} / {byte_order} / {decimals}位小数 (0x{descriptor:04X})"
    if kind == "bcd_datetime":
        digits = logical.hex().upper()
        if not re.fullmatch(r"\d*", digits):
            raise ValueError(f"字段{definition.get('description', definition.get('name'))}包含非法BCD时间：{digits}")
        pattern = str(definition.get("format", "YYMMDDhhmm"))
        if len(pattern) != len(digits):
            raise ValueError(f"字段{definition.get('description', definition.get('name'))}的时间格式长度不匹配")
        parts = {token: digits[index:index + 2] for token, index in (("YY", pattern.find("YY")), ("MM", pattern.find("MM")), ("DD", pattern.find("DD")), ("hh", pattern.find("hh")), ("mm", pattern.find("mm")), ("ss", pattern.find("ss"))) if index >= 0}
        date = "-".join(parts[token] for token in ("YY", "MM", "DD") if token in parts)
        clock = ":".join(parts[token] for token in ("hh", "mm", "ss") if token in parts)
        return f"{date} {clock}".strip()
    if kind == "bcd":
        mutable = bytearray(logical)
        negative = _apply_sign_bit(mutable, definition)
        digits = mutable.hex().upper()
        if not re.fullmatch(r"\d*", digits):
            raise ValueError(f"字段{definition.get('description', definition.get('name'))}包含非法BCD数字：{digits}")
        decimals = int(definition.get("decimals", 0))
        number = Decimal(int(digits or "0")) / (Decimal(10) ** decimals)
        value: Any = -number if negative else number
    elif kind in ("uint", "unsigned_integer", "enum", "bit_field"):
        value = int.from_bytes(raw_value, order, signed=False)
    elif kind in ("int", "signed_integer"):
        value = int.from_bytes(raw_value, order, signed=True)
    elif kind in ("float32", "float_32", "float64", "float_64"):
        if length := len(raw_value):
            expected = 4 if kind in ("float32", "float_32") else 8
            if length != expected:
                raise ValueError(f"浮点字段长度应为{expected}字节")
        value = struct.unpack(("<" if order == "little" else ">") + ("f" if len(raw_value) == 4 else "d"), raw_value)[0]
    elif kind == "ascii":
        value = raw_value.decode(str(definition.get("encoding", "ascii"))).rstrip("\x00")
    elif kind in ("hex", "raw_bytes"):
        return raw_value.hex().upper()
    else:
        raise ValueError(f"不支持的数据类型：{kind}")

    if kind not in ("enum", "bit_field", "ascii"):
        value = Decimal(value) * _decimal(definition.get("scale", 1), "scale") + _decimal(definition.get("offset", 0), "offset")
    if kind == "enum":
        values = {str(key): text for key, text in definition.get("values", {}).items()}
        return values.get(str(value), f"未知({value})")
    if kind == "bit_field":
        bits = definition.get("bits", {})
        active = [str(text) for bit, text in bits.items() if value & (1 << int(bit))]
        return "、".join(active) if active else "0"
    if isinstance(value, Decimal):
        return format(value, "f")
    return value


def encode_field(value: Any, definition: Mapping[str, Any]) -> bytes:
    kind = str(definition.get("type", "hex")).lower()
    length = int(definition["length"])
    order = _byte_order(definition)
    label = str(definition.get("description", definition.get("name", "字段")))
    if kind == "type_descriptor":
        if isinstance(value, Mapping):
            data_type_text = str(value.get("data_type", "uint_16"))
            byte_order_text = str(value.get("byte_order", "ABCD")).upper()
            decimals = int(value.get("decimals", 0))
        else:
            parts = [part.strip() for part in re.split(r"[,|/]", str(value))]
            if len(parts) != 3:
                raise ValueError(f"{label}应包含数据类型、字节序和小数位数")
            data_type_text, byte_order_text, decimals_text = parts
            byte_order_text = byte_order_text.upper()
            decimals = int(decimals_text)
        type_codes = {text.lower(): code for code, text in DESCRIPTOR_DATA_TYPES.items()}
        order_codes = {text: code for code, text in DESCRIPTOR_BYTE_ORDERS.items()}
        if data_type_text.lower() not in type_codes:
            raise ValueError(f"{label}的数据类型无效：{data_type_text}")
        if byte_order_text not in order_codes:
            raise ValueError(f"{label}的字节序无效：{byte_order_text}")
        if not 0 <= decimals <= 5:
            raise ValueError(f"{label}的小数位必须为0~5")
        descriptor = type_codes[data_type_text.lower()] | (order_codes[byte_order_text] << 4) | (decimals << 8)
        return descriptor.to_bytes(length, order, signed=False)
    if kind == "bcd_datetime":
        digits = re.sub(r"\D", "", str(value))
        pattern = str(definition.get("format", "YYMMDDhhmm"))
        if len(digits) != len(pattern) or len(digits) != length * 2:
            raise ValueError(f"{label}必须按{pattern}填写")
        logical = bytes.fromhex(digits)
        return logical[::-1] if order == "little" else logical
    if kind in ("bcd", "uint", "unsigned_integer", "int", "signed_integer"):
        candidate = _decimal(value, label)
        if "minimum" in definition and candidate < _decimal(definition["minimum"], "minimum"):
            raise ValueError(f"{label}不能小于{definition['minimum']}")
        if "maximum" in definition and candidate > _decimal(definition["maximum"], "maximum"):
            raise ValueError(f"{label}不能大于{definition['maximum']}")
    if kind == "bcd":
        number = _decimal(value, label)
        negative = number < 0
        number = abs(number)
        number = (number - _decimal(definition.get("offset", 0), "offset")) / _decimal(definition.get("scale", 1), "scale")
        scaled = number * (Decimal(10) ** int(definition.get("decimals", 0)))
        if scaled != scaled.to_integral_value():
            raise ValueError(f"{label}的小数位超过配置限制")
        digits = str(int(scaled)).zfill(length * 2)
        if len(digits) > length * 2:
            raise ValueError(f"{label}超出{length}字节BCD范围")
        logical = bytearray.fromhex(digits)
        if negative:
            sign = definition.get("sign_bit")
            if not sign:
                raise ValueError(f"{label}不允许负数")
            index = int(sign.get("byte", len(logical) - 1))
            logical[index] |= int(str(sign.get("mask", "80")), 16)
        return bytes(logical[::-1] if order == "little" else logical)
    if kind == "enum":
        values = definition.get("values", {})
        reverse = {str(text): int(key) for key, text in values.items()}
        integer = reverse.get(str(value), int(value) if str(value).lstrip("-").isdigit() else None)
        if integer is None:
            raise ValueError(f"{label}不是有效枚举值")
        return int(integer).to_bytes(length, order, signed=False)
    if kind in ("uint", "unsigned_integer", "bit_field", "int", "signed_integer"):
        number = (_decimal(value, label) - _decimal(definition.get("offset", 0), "offset")) / _decimal(definition.get("scale", 1), "scale")
        if number != number.to_integral_value():
            raise ValueError(f"{label}不能编码为整数")
        return int(number).to_bytes(length, order, signed=kind in ("int", "signed_integer"))
    if kind in ("float32", "float_32", "float64", "float_64"):
        expected = 4 if kind in ("float32", "float_32") else 8
        if length != expected:
            raise ValueError(f"{label}长度必须为{expected}字节")
        return struct.pack(("<" if order == "little" else ">") + ("f" if expected == 4 else "d"), float(value))
    if kind == "ascii":
        raw = str(value).encode(str(definition.get("encoding", "ascii")))
        if len(raw) > length:
            raise ValueError(f"{label}超过{length}字节")
        return raw.ljust(length, b"\x00")
    if kind in ("hex", "raw_bytes"):
        cleaned = re.sub(r"[\s-]", "", str(value))
        if not re.fullmatch(rf"[0-9A-Fa-f]{{{length * 2}}}", cleaned):
            raise ValueError(f"{label}必须是{length * 2}位十六进制字符")
        return bytes.fromhex(cleaned)
    raise ValueError(f"不支持的数据类型：{kind}")


@dataclass(frozen=True)
class DataIdentifierDefinition:
    di: str
    description: str
    access: str
    read_response: Mapping[str, Any] = field(default_factory=dict)
    write_request: Mapping[str, Any] = field(default_factory=dict)
    category: str = "standard"
    category_description: str = "标准DL/T 645"
    group_id: str = "standard"
    group_description: str = "标准数据标识"
    selector: str = ""


class DataIdentifierRegistry:
    def __init__(self, definitions: Iterable[DataIdentifierDefinition], defaults: Optional[Mapping[str, Any]] = None,
                 categories: Optional[Mapping[str, str]] = None) -> None:
        self.definitions = {item.di: item for item in definitions}
        self.defaults = dict(defaults or {})
        self.categories = dict(categories or {"standard": "标准DL/T 645", "extended": "扩展DL/T 645"})

    @classmethod
    def load(cls, path: Path | str) -> "DataIdentifierRegistry":
        source = Path(path)
        try:
            document = json.loads(source.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise ValueError(f"无法加载数据标识配置 {source}：{exc}") from exc
        if document.get("protocol") != "DLT645-2007":
            raise ValueError("配置文件protocol必须为DLT645-2007")
        category_map = {str(item["id"]): str(item.get("description", item["id"]))
                        for item in document.get("categories", []) if isinstance(item, dict) and "id" in item}
        category_map.setdefault("standard", "标准DL/T 645")
        category_map.setdefault("extended", "扩展DL/T 645")
        schemas = document.get("schemas", {})
        if not isinstance(schemas, dict):
            raise ValueError("schemas必须是对象")

        def section(raw: Mapping[str, Any], direct_name: str, reference_name: str) -> Mapping[str, Any]:
            value = raw.get(direct_name, raw.get(reference_name, {}))
            if isinstance(value, str):
                if value not in schemas or not isinstance(schemas[value], dict):
                    raise ValueError(f"结构体{value}不存在")
                return schemas[value]
            if not isinstance(value, dict):
                raise ValueError(f"{direct_name}必须是对象或结构体名称")
            return value

        definitions: list[DataIdentifierDefinition] = []
        seen: set[str] = set()
        for index, raw in enumerate(document.get("data_identifiers", []), 1):
            try:
                di = normalize_di(str(raw["di"]))
                description = str(raw["description"])
                access = str(raw.get("access", "read")).lower()
            except (KeyError, ValueError) as exc:
                raise ValueError(f"第{index}个数据标识配置无效：{exc}") from exc
            if access not in ("read", "write", "read_write"):
                raise ValueError(f"数据标识{di}的access必须为read、write或read_write")
            if di in seen:
                raise ValueError(f"数据标识{di}重复")
            seen.add(di)
            read_response = section(raw, "read_response", "read_schema")
            write_request = section(raw, "write_request", "write_schema")
            for section_name, configured_section in (("read_response", read_response), ("write_request", write_request)):
                names: set[str] = set()
                for field_index, field_def in enumerate(configured_section.get("fields", []), 1):
                    name = str(field_def.get("name", ""))
                    if not name or name in names:
                        raise ValueError(f"数据标识{di}的{section_name}第{field_index}个字段名称为空或重复")
                    names.add(name)
                    if int(field_def.get("length", 0)) <= 0:
                        raise ValueError(f"数据标识{di}字段{name}的length必须大于0")
            category = str(raw.get("category", "standard"))
            definitions.append(DataIdentifierDefinition(
                di, description, access, read_response, write_request, category,
                category_map.get(category, category), str(raw.get("group_id", "standard")),
                str(raw.get("group_description", "标准数据标识")), str(raw.get("selector", di[-2:])),
            ))

        for group_index, raw in enumerate(document.get("data_identifier_groups", []), 1):
            try:
                prefix = re.sub(r"[\s-]", "", str(raw["prefix"])).upper()
                if not re.fullmatch(r"[0-9A-F]{6}", prefix):
                    raise ValueError("prefix必须为3字节十六进制")
                group_id = str(raw.get("id", prefix))
                group_description = str(raw["description"])
                category = str(raw.get("category", "extended"))
                access = str(raw.get("access", "read")).lower()
                suffix_config = raw["suffixes"]
                if not isinstance(suffix_config, dict):
                    raise ValueError("suffixes必须是对象")
                suffixes: list[str] = []
                ranges = suffix_config.get("ranges", [])
                if "range" in suffix_config:
                    ranges = [suffix_config["range"], *ranges]
                for configured_range in ranges:
                    start = int(str(configured_range["from"]), 16)
                    end = int(str(configured_range["to"]), 16)
                    if not 0 <= start <= end <= 255:
                        raise ValueError("后缀范围必须在00~FF且起点不大于终点")
                    suffixes.extend(f"{value:02X}" for value in range(start, end + 1))
                suffixes.extend(f"{int(str(value), 16):02X}" for value in suffix_config.get("values", []))
                read_response = section(raw, "read_response", "read_schema")
                write_request = section(raw, "write_request", "write_schema")
            except (KeyError, ValueError, TypeError) as exc:
                raise ValueError(f"第{group_index}个数据标识组配置无效：{exc}") from exc
            labels = {str(key).upper(): str(value) for key, value in suffix_config.get("labels", {}).items()}
            template = str(suffix_config.get("label_template", group_description + "{decimal}"))
            for suffix in dict.fromkeys(suffixes):
                di = prefix + suffix
                if di in seen:
                    raise ValueError(f"数据标识{di}重复")
                seen.add(di)
                description = labels.get(suffix, template.format(decimal=int(suffix, 16), hex=suffix, suffix=suffix))
                definitions.append(DataIdentifierDefinition(
                    di, description, access, read_response, write_request, category,
                    category_map.get(category, category), group_id,
                    str(raw.get("display", f"{prefix[:2]} {prefix[2:4]} {prefix[4:]} xx | {group_description}")), suffix,
                ))
        return cls(definitions, document.get("defaults", {}), category_map)

    def get(self, data_identifier: str) -> Optional[DataIdentifierDefinition]:
        return self.definitions.get(normalize_di(data_identifier))

    def encode(self, data_identifier: str, values: Mapping[str, Any], section: str = "write_request") -> bytes:
        definition = self.get(data_identifier)
        if not definition:
            raise ValueError(f"未配置数据标识{data_identifier}")
        schema = getattr(definition, section)
        result = bytearray()
        for field_def in schema.get("fields", []):
            name = str(field_def["name"])
            if name not in values:
                raise ValueError(f"缺少字段：{field_def.get('description', name)}")
            result.extend(encode_field(values[name], field_def))
        return bytes(result)

    def decode(self, data_identifier: str, payload: bytes, section: str = "read_response") -> list[tuple[str, str, str]]:
        definition = self.get(data_identifier)
        if not definition:
            return [("raw", payload.hex().upper(), "")]
        schema = getattr(definition, section)
        result: list[tuple[str, str, str]] = []
        offset = 0
        for field_def in schema.get("fields", []):
            length = int(field_def["length"])
            if offset + length > len(payload):
                raise ValueError(f"数据标识{definition.di}的数据不足，字段{field_def.get('description', field_def['name'])}需要{length}字节")
            value = decode_field(payload[offset:offset + length], field_def)
            result.append((str(field_def.get("description", field_def["name"])), str(value), str(field_def.get("unit", ""))))
            offset += length
        if offset < len(payload):
            result.append(("未定义尾部数据", payload[offset:].hex().upper(), ""))
        return result
