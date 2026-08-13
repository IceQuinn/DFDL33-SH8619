import queue
import threading
import unittest

import serial

from app import SerialAssistant


class FakePort:
    bytesize = 8
    parity = serial.PARITY_NONE
    stopbits = serial.STOPBITS_ONE
    baudrate = 9600

    def __init__(self, incoming: bytes = b"") -> None:
        self.incoming = bytearray(incoming)
        self.written = bytearray()

    @property
    def in_waiting(self) -> int:
        return len(self.incoming)

    def read(self, size: int) -> bytes:
        data = bytes(self.incoming[:size])
        del self.incoming[:size]
        return data

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)


class ProxyReaderTests(unittest.TestCase):
    def test_master_frame_is_forwarded_and_reported_once(self):
        frame = bytes.fromhex("01 03 00 00 00 02 C4 0B")
        source = FakePort(frame)
        target = FakePort()
        assistant = type("ReaderContext", (), {})()
        assistant.stop_event = threading.Event()
        assistant.events = queue.Queue()

        thread = threading.Thread(
            target=SerialAssistant._reader,
            args=(assistant, source, "MASTER", target),
            daemon=True,
        )
        thread.start()
        kind, payload = assistant.events.get(timeout=1)
        assistant.stop_event.set()
        thread.join(timeout=1)

        self.assertEqual(kind, "frame")
        self.assertEqual(payload, ("MASTER", frame))
        self.assertEqual(bytes(target.written), frame)
        self.assertTrue(assistant.events.empty())


if __name__ == "__main__":
    unittest.main()
