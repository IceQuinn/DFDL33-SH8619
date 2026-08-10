import unittest

from modbus_rtu import crc16, parse_hex, parse_master_request


def with_crc(payload: bytes) -> bytes:
    checksum = crc16(payload)
    return payload + bytes((checksum & 0xFF, checksum >> 8))


class ModbusParserTests(unittest.TestCase):
    def test_read_holding_registers(self):
        result = parse_master_request(parse_hex("01 03 00 00 00 02 C4 0B"))
        self.assertTrue(result.valid)
        self.assertTrue(result.crc_ok)
        self.assertEqual((result.device_address, result.operation), (1, "读"))
        self.assertEqual((result.register_address, result.register_count), (0, 2))

    def test_write_multiple_registers(self):
        frame = with_crc(bytes.fromhex("0A 10 01 00 00 02 04 12 34 56 78"))
        result = parse_master_request(frame)
        self.assertTrue(result.crc_ok)
        self.assertEqual((result.register_address, result.register_count), (0x100, 2))
        self.assertEqual(result.write_values, (0x1234, 0x5678))

    def test_bad_crc(self):
        result = parse_master_request(bytes.fromhex("01 06 00 01 00 03 00 00"))
        self.assertFalse(result.crc_ok)
        self.assertEqual(result.operation, "写")

    def test_read_write_multiple(self):
        frame = with_crc(bytes.fromhex("01 17 00 10 00 02 00 20 00 02 04 00 01 00 02"))
        result = parse_master_request(frame)
        self.assertEqual(result.operation, "读/写")
        self.assertEqual((result.read_address, result.read_count), (0x10, 2))
        self.assertEqual((result.register_address, result.register_count), (0x20, 2))


if __name__ == "__main__":
    unittest.main()
