import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from inspect_boot import inspect_surviving_boot


class SurvivingBoot(unittest.TestCase):
    def test_captures_and_skip(self):
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "main.bin"
            data = bytearray(b"\xff") * 0x84200
            data[0x20000:0x20200] = b"\x00" * 512
            struct.pack_into("<I", data, 0x40000, 0x84000)
            data[0x84000:0x84200] = bytes(range(256)) * 2
            dump.write_bytes(data)
            report = inspect_surviving_boot(dump, 64)
            self.assertIn("0x020000 + 0x200: all 00", report)
            self.assertIn("0x080000 + 0x200: all FF", report)
            self.assertIn("0x084000 + 0x200: CRC32", report)
            self.assertIn(hashlib.sha256(data[0x84000:0x84200]).hexdigest(), report)
            self.assertNotIn("0x020000", inspect_surviving_boot(dump, 128))

            data[0x3fffc:0x40000] = b"QAFE"
            dump.write_bytes(data[:0x80000])
            report = inspect_surviving_boot(dump, 128)
            self.assertNotIn("0x084000", report)
            self.assertIn("0x080000 + 0x200: outside supplied dump", report)

    def test_volume_is_not_raw(self):
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "disk.bin"
            dump.write_bytes(b"\x00" * 3 + b"DSGBAFLS")
            with self.assertRaisesRegex(ValueError, "physical main"):
                inspect_surviving_boot(dump, 128)


if __name__ == "__main__":
    unittest.main()
