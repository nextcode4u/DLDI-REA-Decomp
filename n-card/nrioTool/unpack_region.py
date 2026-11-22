#!/usr/bin/env python3
"""Verify a named nrioTool region backup, optionally extracting its blocks."""

import argparse
import struct
import zlib
from pathlib import Path

SECTOR_BYTES = 512
PAGE_BYTES = 2048
SPARE_BYTES = 64
RECORD_BYTES = PAGE_BYTES + SPARE_BYTES
HEADER = struct.Struct("<4I")


def read_blocks(path):
    with path.open("rb") as backup:
        if backup.read(8) != b"NRIOREG1":
            raise ValueError("Unrecognized region backup signature.")
        count = backup.read(4)
        if len(count) != 4:
            raise ValueError("Truncated backup header.")
        count, = struct.unpack("<I", count)
        if not 1 <= count <= 16:
            raise ValueError("Invalid saved block count.")
        seen = set()
        identity = None
        for _ in range(count):
            header = backup.read(HEADER.size)
            if len(header) != HEADER.size:
                raise ValueError("Truncated block header.")
            lba, nand_id, size, stored_crc = HEADER.unpack(header)
            if size not in (64 * RECORD_BYTES, 128 * RECORD_BYTES):
                raise ValueError("Invalid erase-block size.")
            pages = size // RECORD_BYTES
            if identity is None:
                identity = nand_id, pages
            if identity != (nand_id, pages) or lba % (pages * PAGE_BYTES // SECTOR_BYTES) or lba in seen:
                raise ValueError("Inconsistent NAND identity, geometry, or block address.")
            records = backup.read(size)
            if len(records) != size or zlib.crc32(records) ^ 0xffffffff != stored_crc:
                raise ValueError(f"Incomplete or corrupt block at 0x{lba * SECTOR_BYTES:X}.")
            seen.add(lba)
            yield lba, nand_id, records
        if backup.read(1):
            raise ValueError("Unexpected data past the last backup block.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("backup", type=Path)
    parser.add_argument("--out", type=Path, help="extract to a new directory")
    args = parser.parse_args()
    try:
        blocks = list(read_blocks(args.backup))
        if args.out:
            args.out.mkdir()
        for lba, nand_id, records in blocks:
            address = lba * SECTOR_BYTES
            pages = len(records) // RECORD_BYTES
            print(f"0x{address:08X}  NAND {nand_id:08X}  {pages} pages  CRC OK")
            if args.out:
                main_data = bytearray()
                spare_data = bytearray()
                for offset in range(0, len(records), RECORD_BYTES):
                    main_data.extend(records[offset:offset + PAGE_BYTES])
                    spare_data.extend(records[offset + PAGE_BYTES:offset + RECORD_BYTES])
                (args.out / f"{address:08X}-main.bin").write_bytes(main_data)
                (args.out / f"{address:08X}-spare.bin").write_bytes(spare_data)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
