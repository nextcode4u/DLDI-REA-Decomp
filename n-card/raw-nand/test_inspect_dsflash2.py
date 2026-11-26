import random
import struct
import tempfile
import unittest
from pathlib import Path

from inspect_dsflash2 import compare_spare, crc16, inspect_image, inspect_main, parse_header


def image_fixture():
    image = bytearray(0x8000)
    image[:16] = b"DSGBA PASS\0\0DSGB"
    rng = random.Random(12)
    image[0x1000:0x1048] = rng.randbytes(72)
    image[0x2000:0x3000] = rng.randbytes(4096)
    image[0x4000:] = rng.randbytes(0x4000)
    struct.pack_into("<H", image, 0x6C, crc16(image[0x4000:]))
    # This final word creates the expected Nintendo logo CRC without
    # requiring a real Nintendo ROM in the repository.
    image[0x15A:0x15C] = bytes.fromhex("04 68")
    struct.pack_into("<H", image, 0x15C, crc16(image[0xC0:0x15C]))
    struct.pack_into("<H", image, 0x15E, crc16(image[:0x15E]))
    return image


class Inspector(unittest.TestCase):
    def test_generated_and_corrupt(self):
        image = image_fixture()
        self.assertEqual(parse_header(image)["code"], "DSGB")
        self.assertTrue(inspect_image(image)[1])
        image[-1] ^= 1
        self.assertFalse(inspect_image(image)[1])
        image[0] ^= 1
        self.assertIsNone(parse_header(image))

    def test_yjloader_is_not_an_nrio_image(self):
        image = image_fixture()
        image[:16] = b"YJLOADER" + b"\0"*4 + b"YJYJ"
        struct.pack_into("<H", image, 0x15E, crc16(image[:0x15E]))
        lines, generated = inspect_image(image)
        self.assertIn("YJLoader-family header", "\n".join(lines))
        self.assertFalse(generated)
        image[0] ^= 1
        self.assertNotIn("YJLoader-family header", "\n".join(inspect_image(image)[0]))

    def test_wrapped_image(self):
        image = image_fixture()
        inner = bytearray(image[:0x200])
        struct.pack_into("<I", inner, 0x80, 0x1000)
        struct.pack_into("<4I", inner, 0x20, 0x400, 0, 0, 0x300)
        struct.pack_into("<4I", inner, 0x30, 0x800, 0, 0, 0x300)
        struct.pack_into("<H", inner, 0x15E, crc16(inner[:0x15E]))
        image.extend(inner)
        self.assertIn("Looks like a wrapped image", "\n".join(inspect_image(image, 0x9000)[0]))
        self.assertIn("does not contain the full nested ROM", "\n".join(inspect_image(image)[0]))

    def test_spare_order(self):
        image = image_fixture()
        spare = bytearray([255]) * 512
        spare[0x80:0xC0] = image[0x1008:0x1048]
        spare[0xC0:0xC8] = image[0x1000:0x1008]
        self.assertIn("72/72", compare_spare(image, spare))
        spare[0x90] ^= 1
        self.assertIn("71/72", compare_spare(image, spare))
        self.assertIn("Too short", compare_spare(image, spare[:64]))

    def test_raw_map_and_rom_occupancy(self):
        image = image_fixture()
        main = bytearray([255]) * 0x50000
        main[0x400:0x410] = b"DSGB" + b"\x00" * 8 + b"DSGB"
        struct.pack_into("<I", main, 0x8000, 0x40000)
        main[0x40000:0x40200] = image[:0x200]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "main.bin"
            path.write_bytes(main)
            report = "\n".join(inspect_main(path, image))
            self.assertIn("0x40000; candidate header matches", report)
            self.assertIn("Table 0x40000: DS ROM header occupies", report)
            main[0x40000:0x40020] = b"<<GAME>>" * 4
            path.write_bytes(main)
            self.assertIn("single-game marker occupies", "\n".join(inspect_main(path, image)))
            path.write_bytes(main[:0x20000])
            self.assertIn("header outside supplied dump", "\n".join(inspect_main(path, image)))
            path.write_bytes(image)
            self.assertIn("rather than a logical .nds", inspect_main(path, image)[0])


if __name__ == "__main__":
    unittest.main()
