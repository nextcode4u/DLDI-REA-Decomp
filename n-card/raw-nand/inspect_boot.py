#!/usr/bin/env python3
"""Inspect boot regions in separate NRIO main and spare dumps."""

import argparse
import hashlib
import struct
from pathlib import Path


BOOT_ROM_HEADER = 0x400
PRIMARY_TABLE = 0x8000
SECONDARY_TABLE = 0x40000
TABLE_SIZE = 0x4000
REPORTED_MAX_TABLE_SIZE = 0x10000
MAPPED_BLOCK_SIZE = 0x4000
SECTOR_SIZE = 512
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


def inspect_udisk(main_dump, table):
    """Check the known compact header and the sectors used by its mapping."""
    entries = [entry for (entry,) in struct.iter_unpack("<I", table)]
    first = entries[0]
    if first < USUAL_UDISK_START or first % MAPPED_BLOCK_SIZE:
        return f"Invalid first mapped address 0x{first:X}.", None

    with main_dump.open("rb") as dump:
        dump.seek(first)
        header = dump.read(SECTOR_SIZE)
        if len(header) < SECTOR_SIZE:
            return f"Dump does not reach the mapped header at 0x{first:X}.", None
        if (header[:4] != bytes.fromhex("2e 00 00 ea") or
                header[0x0C:0x10] != b"####" or header[0x14] != 3):
            return f"Unrecognized compact uDisk header at 0x{first:X}.", None

        size, = struct.unpack_from("<I", header, 0x80)
        arm9, = struct.unpack_from("<I", header, 0x20)
        arm9_size, = struct.unpack_from("<I", header, 0x2C)
        arm7, = struct.unpack_from("<I", header, 0x30)
        arm7_size, = struct.unpack_from("<I", header, 0x3C)
        if not all(offset >= SECTOR_SIZE and offset % SECTOR_SIZE == 0 and
                   length > 0 and offset + length <= size
                   for offset, length in ((arm9, arm9_size), (arm7, arm7_size))):
            return "Compact header has invalid ARM9 or ARM7 ranges.", None

        used = (size + MAPPED_BLOCK_SIZE - 1) // MAPPED_BLOCK_SIZE
        if used > len(entries):
            return f"Header needs {used} entries beyond the inspected table range.", None
        previous = 0
        for index, address in enumerate(entries[:used]):
            if address < USUAL_UDISK_START or address % MAPPED_BLOCK_SIZE or address <= previous:
                return f"Invalid mapping entry {index} at 0x{address:X}.", None
            previous = address

        for label, offset in (("ARM9", arm9), ("ARM7", arm7)):
            address = entries[offset // MAPPED_BLOCK_SIZE] + offset % MAPPED_BLOCK_SIZE
            dump.seek(address)
            code = dump.read(SECTOR_SIZE)
            if len(code) != SECTOR_SIZE:
                return f"Dump does not reach the mapped {label} sector at 0x{address:X}.", None
            if len(set(code[:16])) == 1:
                return f"Mapped {label} sector at 0x{address:X} has uniform leading bytes.", None

    return (f"Recognized header at 0x{first:X}. Checked {used} map entries and "
            "the first ARM9 and ARM7 sectors."), used


def count_differences(primary, secondary):
    return sum(primary[offset:offset + SECTOR_SIZE] != secondary[offset:offset + SECTOR_SIZE]
               for offset in range(0, len(primary), SECTOR_SIZE))


def inspect_safe_mode(main_dump, spare_dump, original_path, patched_path, pages_per_block):
    """Verify the primary map against an extracted original and report erase coverage."""
    main = read_main_dump(main_dump, PRIMARY_TABLE + REPORTED_MAX_TABLE_SIZE)
    if main[PRIMARY_TABLE - 4:PRIMARY_TABLE] == b"QAFE":
        raise ValueError("QAFE precedes the primary table. Stage 2 would skip it.")
    entries = [address for (address,) in struct.iter_unpack("<I", main[PRIMARY_TABLE:])]
    original = original_path.read_bytes()
    if len(original) < 0x200:
        raise ValueError("The original ROM is shorter than its header.")
    size, = struct.unpack_from("<I", original, 0x80)
    if not 0x200 <= size <= min(len(original), len(entries) * MAPPED_BLOCK_SIZE):
        raise ValueError("Original ROM size is missing or exceeds the available table entries.")
    patched = patched_path.read_bytes() if patched_path else None
    if patched is not None and (len(patched) != len(original) or patched[size:] != original[size:]):
        raise ValueError("The patched ROM must preserve the file length and bytes past its declared size.")

    changed_pages = set()
    rom_blocks = set()
    seen = set()
    block_bytes = pages_per_block * PAGE_SIZE
    with main_dump.open("rb") as dump:
        for offset in range(0, size, MAPPED_BLOCK_SIZE):
            index = offset // MAPPED_BLOCK_SIZE
            address = entries[index]
            if address < MAPPED_BLOCK_SIZE or address % MAPPED_BLOCK_SIZE or address in seen:
                raise ValueError(f"Invalid primary map entry {index} at 0x{address:X}.")
            seen.add(address)
            expected = original[offset:min(offset + MAPPED_BLOCK_SIZE, size)]
            dump.seek(address)
            actual = dump.read(len(expected))
            if len(actual) != len(expected):
                raise ValueError(f"Main backup ends before the mapped ROM range at 0x{address:X}.")
            if actual != expected:
                raise ValueError(f"Original ROM differs from NAND at mapped entry {index} (0x{address:X}).")

            rom_blocks.add(address // block_bytes)
            if patched is not None:
                new = patched[offset:offset + len(expected)]
                for page_offset in range(0, len(expected), PAGE_SIZE):
                    span = slice(page_offset, page_offset + PAGE_SIZE)
                    if new[span] != expected[span]:
                        changed_pages.add((address + page_offset) // PAGE_SIZE)

    lines = [f"Primary table at 0x{PRIMARY_TABLE:X} maps 0x{size:X} bytes matching the original ROM.",
             f"Erase geometry: {pages_per_block} pages per block (0x{block_bytes:X} main bytes)."]
    blocks = {page // pages_per_block for page in changed_pages} if patched is not None else rom_blocks
    if patched is not None:
        lines.append(f"Changed physical NAND pages: {len(changed_pages)}")
    main_length = main_dump.stat().st_size
    if not spare_dump.is_file():
        raise ValueError(f"{spare_dump} must be a packed spare dump file")
    spare_length = spare_dump.stat().st_size
    for block in sorted(blocks):
        start = block * block_bytes
        end = start + block_bytes
        spare_start = block * pages_per_block * SPARE_SIZE
        spare_end = spare_start + pages_per_block * SPARE_SIZE
        lines.append(f"  Erase block {block}: main 0x{start:X}–0x{end - 1:X}, "
                     f"packed spare 0x{spare_start:X}–0x{spare_end - 1:X}")
        if main_length < end or spare_length < spare_end:
            lines.append("    Backup is missing part of this erase block.")
    lines.append("Read-only comparison. No flash image was created.")
    return "\n".join(lines)


def inspect_boot(main_dump, spare_dump):
    main_data = read_main_dump(main_dump, SECONDARY_TABLE + REPORTED_MAX_TABLE_SIZE)
    spare_data = read_prefix(spare_dump, SPARE_SECTOR_SIZE)
    primary = main_data[PRIMARY_TABLE:PRIMARY_TABLE + REPORTED_MAX_TABLE_SIZE]
    secondary = main_data[SECONDARY_TABLE:SECONDARY_TABLE + REPORTED_MAX_TABLE_SIZE]
    lines = ["Main NAND (reported uDisk-era offsets)", f"  Page 0 at 0x0000  {describe(main_data[:PAGE_SIZE])}"]
    header = main_data[BOOT_ROM_HEADER:BOOT_ROM_HEADER + 0x10]
    if header[:8] == b"DSGBA PA" and header[0x0C:0x10] == b"DSGB":
        title_bytes = header[8:12]
        word = int.from_bytes(title_bytes, "little")
        lines.append(f"  DSGB title bytes at 0x{BOOT_ROM_HEADER + 8:04X}  "
                     f"{title_bytes.hex(' ').upper()} (LE 0x{word:08X})")
    else:
        lines.append(f"  No DSGBA PA / DSGB header match at 0x{BOOT_ROM_HEADER:04X}.")
    for offset, table in ((PRIMARY_TABLE, primary), (SECONDARY_TABLE, secondary)):
        lines.append(f"  0x{offset:05X}  64 KiB candidate region  {describe(table)}")
        lines.append(f"    SHA-256 {hashlib.sha256(table).hexdigest()}")
    base_diff = count_differences(primary[:TABLE_SIZE], secondary[:TABLE_SIZE])
    tail_diff = count_differences(primary[TABLE_SIZE:], secondary[TABLE_SIZE:])
    lines.append(f"  Differing sectors in first 16 KiB  {base_diff} of 32")
    lines.append(f"  Differing sectors in remaining 48 KiB  {tail_diff} of 96")

    marker = main_data[SECONDARY_TABLE - 4:SECONDARY_TABLE]
    lines.append("uDisk candidate mapped by the secondary table")
    if marker == b"QAFE":
        lines.append("  QAFE precedes the table. Stage 2 would skip it.")
    else:
        result, used = inspect_udisk(main_dump, secondary)
        lines.append(f"  {result}")
        if used is not None:
            required_bytes = used * 4
            lines.append(f"  Mapped image needs 0x{required_bytes:X} bytes of entries "
                         f"({(required_bytes + SECTOR_SIZE - 1) // SECTOR_SIZE} table sectors).")
            active_diff = count_differences(primary[:required_bytes], secondary[:required_bytes])
            lines.append(f"  Sectors with differing entries used by this image  {active_diff}")

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

    marker0, marker1 = spare_data[0], spare_data[SPARE_SIZE]
    lines.append(f"  Page 0 and 1 first spare bytes  0x{marker0:02X}, 0x{marker1:02X}")
    if any(marker not in (0xFF, 0x55) for marker in (marker0, marker1)):
        lines.append("  General main/spare writes would stop at the block-0 spare preflight.")
    arm7 = spare_data[SPARE_SIZE:2 * SPARE_SIZE]
    boot_data = spare_data[2 * SPARE_SIZE:2 * SPARE_SIZE + 56]
    records_match = (
        arm7[:8] == bytes.fromhex("00 00 a0 e3 20 10 9f e5")
        and arm7 == spare_data[4 * SPARE_SIZE:5 * SPARE_SIZE]
        and arm7 == spare_data[5 * SPARE_SIZE:6 * SPARE_SIZE]
        and len(set(boot_data)) > 1
        and boot_data == spare_data[3 * SPARE_SIZE + 8:4 * SPARE_SIZE]
    )
    if records_match:
        lines.append("  ARM7 copies and repeated boot data match the known record pattern.")
    else:
        lines.append("  Shared records differ from the known ARM7 and boot-data pattern.")
    if spare_data[:SPARE_SIZE] != b"\xff" * SPARE_SIZE:
        lines.append("  Page 0 spare is populated. The current DS same-cart repair requires it to be blank.")
    lines.append("The 64 KiB range is for comparison. The full table extent and bootability are unverified.")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("main_dump", type=Path, help="main-only dump from Main: read only")
    parser.add_argument("spare_dump", type=Path, help="spare-only dump from Spare: read only")
    parser.add_argument("--safe-mode-rom", type=Path, help="original safe-mode ROM from the target cart")
    parser.add_argument("--patched-rom", type=Path, help="compare a patched copy and list only changed erase blocks")
    parser.add_argument("--pages-per-block", type=int, choices=(64, 128), help="erase-buffer page count from the DS log")
    args = parser.parse_args()
    if args.safe_mode_rom and args.pages_per_block is None:
        parser.error("--safe-mode-rom requires --pages-per-block from the DS log")
    if args.safe_mode_rom is None and (args.patched_rom or args.pages_per_block):
        parser.error("--patched-rom and --pages-per-block require --safe-mode-rom")
    try:
        if args.safe_mode_rom:
            print(inspect_safe_mode(args.main_dump, args.spare_dump, args.safe_mode_rom,
                                    args.patched_rom, args.pages_per_block))
        else:
            print(inspect_boot(args.main_dump, args.spare_dump))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
