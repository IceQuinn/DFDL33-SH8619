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
    encode_field,
    decode_field,
    parse_frame,
    subtract_33,
)
from app import load_settings, save_settings, saved_choice


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
        self.assertEqual(parser.take_unparsed(), b"")
        frames = parser.feed(first[7:] + second)
        self.assertEqual(frames, [first, second])

    def test_stream_parser_exposes_noise_and_partial_frames(self):
        parser = DLT645StreamParser()
        valid = build_frame("123456789012", 0x93)
        self.assertEqual(parser.feed(b"\x01\x02" + valid), [valid])
        self.assertEqual(parser.take_unparsed(), b"\x01\x02")

        partial = bytes.fromhex("FE FE 68 12 34")
        self.assertEqual(parser.feed(partial), [])
        self.assertEqual(parser.take_unparsed(finalize=True), partial)

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

    def test_interface_settings_round_trip_and_invalid_choice_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "settings.json"
            expected = {"mode": "直接收发", "device_port": "COM8", "parity": "偶"}
            save_settings(expected, path)
            self.assertEqual(load_settings(path), expected)
            self.assertEqual(saved_choice(load_settings(path), "parity", "无", ("无", "奇", "偶")), "偶")

            path.write_text("not json", encoding="utf-8")
            self.assertEqual(load_settings(path), {})
            self.assertEqual(saved_choice({"parity": "坏值"}, "parity", "无", ("无", "奇", "偶")), "无")

    def test_extended_identifier_ranges_and_shared_structures(self):
        voltage = self.registry.get("02E60101")
        self.assertEqual(voltage.description, "逆变器1三相电压块")
        self.assertEqual(voltage.access, "read")
        voltage_values = self.registry.decode("02E60101", bytes.fromhex("05 22 01 23 00 24"))
        self.assertEqual(voltage_values, [
            ("A相电压", "220.5", "V"), ("B相电压", "230.1", "V"), ("C相电压", "240", "V"),
        ])
        self.assertEqual(self.registry.get("02E601FF").description, "全部逆变器三相电压块")

        expected_variable_groups = {
            "02E60201": ("逆变器1三相电流块", 9),
            "02E60301": ("逆变器1瞬时有功功率块", 16),
            "02E60401": ("逆变器1瞬时无功功率块", 16),
            "02E60501": ("逆变器1功率因数块", 8),
            "02E60F01": ("逆变器1所有变量数据块", 55),
        }
        for di, (description, payload_length) in expected_variable_groups.items():
            definition = self.registry.get(di)
            self.assertEqual(definition.description, description)
            self.assertEqual(definition.access, "read")
            self.assertEqual(sum(int(field["length"]) for field in definition.read_response["fields"]), payload_length)

        signed_power = self.registry.decode("02E60301", bytes.fromhex(
            "34 12 00 80 00 00 00 00 00 00 00 00 00 00 00 00"
        ))
        self.assertEqual(signed_power[0], ("瞬时总有功功率", "-0.1234", "kW"))

        first = self.registry.get("04E60401")
        last = self.registry.get("04E6040C")
        all_devices = self.registry.get("04E604FF")
        self.assertEqual(first.description, "逆变器1运行状态")
        self.assertEqual(last.selector, "0C")
        self.assertEqual(all_devices.description, "全部逆变器运行状态")
        self.assertEqual(first.category, "extended")
        self.assertEqual(first.access, "read_write")
        self.assertEqual(self.registry.encode("04E60401", {"status": "01"}), bytes.fromhex("01"))

        archive = self.registry.encode("04E62101", {
            "address": "1", "brand": "TEST", "protocol_version": "12", "port": "端口2（RJ45-II）",
        })
        self.assertEqual(len(archive), 36)
        decoded = dict((name, value) for name, value, _unit in self.registry.decode("04E62101", archive))
        self.assertEqual(decoded["逆变器品牌"], "TEST")
        self.assertEqual(decoded["接入端口"], "端口2（RJ45-II）")

    def test_complex_datetime_and_type_descriptor_fields(self):
        datetime_field = {"name": "time", "description": "时间", "type": "bcd_datetime", "length": 5,
                          "format": "YYMMDDhhmm", "byte_order": "big"}
        encoded_time = encode_field("26-09-01 12:34", datetime_field)
        self.assertEqual(encoded_time, bytes.fromhex("26 09 01 12 34"))
        self.assertEqual(decode_field(encoded_time, datetime_field), "26-09-01 12:34")

        descriptor_field = {"name": "type", "description": "TTTT", "type": "type_descriptor",
                            "length": 2, "byte_order": "big"}
        descriptor = encode_field({"data_type": "int_16", "byte_order": "CDAB", "decimals": "2"}, descriptor_field)
        self.assertEqual(descriptor, bytes.fromhex("02 12"))
        self.assertIn("int_16 / CDAB / 2位小数", decode_field(descriptor, descriptor_field))

    def test_all_ff_field_is_displayed_as_invalid(self):
        bcd_field = {"name": "voltage", "description": "电压", "type": "bcd", "length": 2,
                     "byte_order": "little", "decimals": 1}
        ascii_field = {"name": "brand", "description": "品牌", "type": "ascii", "length": 4}
        self.assertEqual(decode_field(bytes.fromhex("FF FF"), bcd_field), "--")
        self.assertEqual(decode_field(bytes.fromhex("FF FF FF FF"), ascii_field), "--")


if __name__ == "__main__":
    unittest.main()
