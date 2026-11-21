#!/usr/bin/env python3
"""Inspect boot regions in separate NRIO main and spare dumps."""

import argparse
import hashlib
from pathlib import Path


PRIMARY_TABLE = 0x8000
SECONDARY_TABLE = 0x40000
TABLE_SIZE = 0x4000
USUAL_UDISK_START = 0x80000
PAGE_SIZE = 0x800
SPARE_SIZE = 64
SPARE_SECTOR_SIZE = 512


def read_prefix(path, size):
    if not path.is_file():
        raise ValueError(f"{path} must be a dump file")
    with path.open("rb") as dump:
        data = dump.read(size)
    if len(data) != size:
        raise ValueError(f"{path} must contain at least 0x{size:X} bytes")
    return data


def describe(data):
    if data.count(0xFF) == len(data):
        return "all FF"
    if data.count(0) == len(data):
        return "all 00"
    return f"{len(data) - data.count(0xFF)} of {len(data)} bytes differ from FF"


def read_main_dump(path, size):
    data = read_prefix(path, size)
    if data[3:11] == b"DSGBAFLS":
        raise ValueError("Main dump begins with a DSGBAFLS volume boot sector. Use a physical main dump.")
    return data


def inspect_boot(main_dump, spare_dump):
    main_data = read_main_dump(main_dump, USUAL_UDISK_START + PAGE_SIZE)
    spare_data = read_prefix(spare_dump, SPARE_SECTOR_SIZE)
    primary = main_data[PRIMARY_TABLE:PRIMARY_TABLE + TABLE_SIZE]
    secondary = main_data[SECONDARY_TABLE:SECONDARY_TABLE + TABLE_SIZE]
    lines = ["Main NAND (reported uDisk-era offsets)", f"  Page 0 at 0x0000  {describe(main_data[:PAGE_SIZE])}"]
    for offset, table in ((PRIMARY_TABLE, primary), (SECONDARY_TABLE, secondary)):
        lines.append(f"  0x{offset:05X} table region  {describe(table)}")
        lines.append(f"    SHA-256 {hashlib.sha256(table).hexdigest()}")
    comparison = "identical" if primary == secondary else "different"
    lines.append(f"  Table region comparison  {comparison}")
    udisk_page = main_data[USUAL_UDISK_START:USUAL_UDISK_START + PAGE_SIZE]
    lines.append(f"  Page at 0x{USUAL_UDISK_START:X} (typical uDisk start)  {describe(udisk_page)}")

    lines.append("Spare USB sector 0")
    first_page_for_record = {}
    for offset in range(0, SPARE_SECTOR_SIZE, SPARE_SIZE):
        page = offset // SPARE_SIZE
        record = spare_data[offset:offset + SPARE_SIZE]
        state = describe(record)
        if len(set(record)) > 1:
            first_page = first_page_for_record.setdefault(record, page)
            if first_page != page:
                state += f", identical to page {first_page} spare"
        lines.append(f"  Page {page} at 0x{offset:03X}  {state}")

    lines.append("Table mappings, uDisk contents, and bootability still require verification.")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("main_dump", type=Path, help="main-only dump from nrio-raw-readonly")
    parser.add_argument("spare_dump", type=Path, help="spare-only dump from nrio-raw-spare-readonly")
    args = parser.parse_args()
    try:
        print(inspect_boot(args.main_dump, args.spare_dump))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
