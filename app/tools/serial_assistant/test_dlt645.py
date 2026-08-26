import tempfile
import unittest
from pathlib import Path

from dlt645 import (
    DLT645StreamParser,
    DataIdentifierDefinition,
    DataIdentifierRegistry,
    add_33,
    build_frame,
    build_read_address,
    build_read_data,
    build_write_data,
    decode_address,
    encode_address,
    encode_di,
    parse_frame,
    subtract_33,
)


CONFIG_PATH = Path(__file__).with_name("dlt645_data_identifiers.json")


class DLT645Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.registry = DataIdentifierRegistry.load(CONFIG_PATH)

    def test_address_byte_order(self):
        encoded = encode_address("123456789012")
        self.assertEqual(encoded, bytes.fromhex("12 90 78 56 34 12"))
        self.assertEqual(decode_address(encoded), "123456789012")

    def test_add_and_subtract_33_round_trip(self):
        clear = bytes.fromhex("00 01 FE FF")
        self.assertEqual(subtract_33(add_33(clear)), clear)

    def test_read_address_request(self):
        frame = build_read_address(4)
        self.assertEqual(frame, bytes.fromhex("FE FE FE FE 68 AA AA AA AA AA AA 68 13 00 DF 16"))
        result = parse_frame(frame)
        self.assertTrue(result.valid)
        self.assertEqual(result.operation, "读通信地址")
        self.assertEqual(result.direction, "主机→从机")

    def test_read_response_is_decoded_from_registry(self):
        clear = encode_di("02010100") + bytes.fromhex("05 22")
        frame = build_frame("123456789012", 0x91, clear)
        result = parse_frame(frame, self.registry)
        self.assertTrue(result.valid)
        self.assertEqual(result.data_identifier, "02010100")
        self.assertEqual(result.decoded_values, (("A相电压", "220.5", "V"),))

    def test_write_request_uses_hidden_security_defaults(self):
        payload = self.registry.encode("F0010001", {"target_voltage": "220.5", "mode": "运行"})
        frame = build_write_data("123456789012", "F0010001", payload)
        result = parse_frame(frame, self.registry)
        self.assertTrue(result.valid)
        self.assertEqual(result.operation, "写数据")
        self.assertEqual(result.data_identifier, "F0010001")
        self.assertEqual(result.payload, payload)

    def test_configured_write_range_is_enforced(self):
        with self.assertRaisesRegex(ValueError, "不能大于23"):
            self.registry.encode("04000102", {"second": 0, "minute": 0, "hour": 24})

    def test_vendor_write_format_can_omit_security_fields(self):
        registry = DataIdentifierRegistry(
            [DataIdentifierDefinition("F1000001", "厂商写入", "write", {}, {
                "fields": [{"name": "value", "type": "uint", "length": 2, "byte_order": "little"}]
            })],
            {"write_include_security": False},
        )
        payload = registry.encode("F1000001", {"value": 0x1234})
        frame = build_write_data(
            "123456789012", "F1000001", payload, include_security=False
        )
        result = parse_frame(frame, registry)
        self.assertTrue(result.valid)
        self.assertEqual(result.payload, bytes.fromhex("34 12"))

    def test_abnormal_response(self):
        frame = build_frame("123456789012", 0xD1, b"\x04")
        result = parse_frame(frame, self.registry)
        self.assertTrue(result.valid)
        self.assertTrue(result.abnormal)
        self.assertIn("密码错误", result.error_text)

    def test_bad_checksum(self):
        frame = bytearray(build_read_data("123456789012", "02010100"))
        frame[-2] ^= 0x01
        result = parse_frame(bytes(frame), self.registry)
        self.assertFalse(result.valid)
        self.assertFalse(result.checksum_ok)

    def test_non_bcd_meter_address_is_invalid(self):
        frame = bytearray(build_frame("123456789012", 0x93))
        frame[1] = 0xFA
        frame[-2] = sum(frame[:-2]) & 0xFF
        result = parse_frame(bytes(frame))
        self.assertFalse(result.valid)
        self.assertIn("不是有效BCD", result.detail)

    def test_stream_parser_handles_preamble_fragmentation_and_sticky_frames(self):
        first = build_read_data("123456789012", "02010100", preamble=4)
        second = build_frame("123456789012", 0x93)
        parser = DLT645StreamParser()
        self.assertEqual(parser.feed(first[:7]), [])
        frames = parser.feed(first[7:] + second)
        self.assertEqual(frames, [first, second])

    def test_follow_up_frame_sequence_is_built_and_parsed(self):
        request = build_read_data("123456789012", "02010100", follow_up=True, sequence=3)
        parsed_request = parse_frame(request)
        self.assertEqual(parsed_request.frame_sequence, 3)
        self.assertEqual(parsed_request.payload, b"")
        response = build_frame("123456789012", 0xB2, encode_di("02010100") + b"\x05\x22\x03")
        parsed_response = parse_frame(response, self.registry)
        self.assertTrue(parsed_response.valid)
        self.assertTrue(parsed_response.follow_up)
        self.assertEqual(parsed_response.frame_sequence, 3)
        self.assertEqual(parsed_response.payload, b"\x05\x22")

    def test_config_can_be_copied_and_reloaded(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "identifiers.json"
            target.write_text(CONFIG_PATH.read_text(encoding="utf-8"), encoding="utf-8")
            registry = DataIdentifierRegistry.load(target)
            self.assertIsNotNone(registry.get("F0010001"))


if __name__ == "__main__":
    unittest.main()
