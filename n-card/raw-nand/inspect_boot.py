#!/usr/bin/env python3
"""Inspect boot regions in separate NRIO main and spare dumps."""

import argparse
import hashlib
import struct
import zlib
from pathlib import Path

from patch_safe_mode import STOCK_STAGE_ADDRESS, crc16, is_compact_safe_mode, patch_stock_rom


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


def check_block0_backup(main_dump, spare_dump, info_path):
    manifest_format = struct.Struct("<8s4I512s512s")
    with info_path.open("rb") as source:
        manifest = source.read(manifest_format.size + 1)
    if len(manifest) != manifest_format.size:
        raise ValueError("backup.info has an unexpected length.")
    tag, nand_id, pages, main_crc, spare_crc, _, anchor_80000 = manifest_format.unpack(manifest)
    if tag != b"NRIOB01\0" or pages not in (64, 128):
        raise ValueError("backup.info has an invalid signature or page count.")

    lines = [f"NAND ID 0x{nand_id:08X}, {pages} pages per erase block"]
    valid = True
    main = b""
    for label, path, size, expected in (
        ("Main", main_dump, pages * PAGE_SIZE, main_crc),
        ("Spare", spare_dump, pages * SPARE_SIZE, spare_crc),
    ):
        with path.open("rb") as source:
            data = source.read(size + 1)
        if label == "Main" and len(data) >= PRIMARY_TABLE + 4:
            main = data
        if len(data) != size:
            lines.append(f"{label}: expected exactly 0x{size:X} bytes.")
            valid = False
            continue
        actual = zlib.crc32(data) ^ 0xFFFFFFFF
        matches = actual == expected
        status = "matches" if matches else "does not match"
        lines.append(f"{label} CRC 0x{actual:08X} {status} manifest 0x{expected:08X}.")
        valid &= matches

    if main:
        address, = struct.unpack_from("<I", main, PRIMARY_TABLE)
        lines.append(f"First primary map entry in the main file: 0x{address:X}.")
        if (address == USUAL_UDISK_START and anchor_80000[:4] == b"\x2e\x00\x00\xea" and
                anchor_80000[12:16] == b"####"):
            size, = struct.unpack_from("<I", anchor_80000, 0x80)
            lines.append(f"The manifest captured a compact ROM header at 0x80000 "
                         f"with declared size 0x{size:X}.")
    return "\n".join(lines), valid


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


def load_primary_rom(main_dump, reference=None):
    """Read the primary table and follow each mapped 16 KiB ROM block."""
    main = read_main_dump(main_dump, PRIMARY_TABLE + REPORTED_MAX_TABLE_SIZE)
    if main[PRIMARY_TABLE - 4:PRIMARY_TABLE] == b"QAFE":
        raise ValueError("QAFE precedes the primary table. Stage 2 would skip it.")
    entries = [address for (address,) in struct.iter_unpack("<I", main[PRIMARY_TABLE:])]
    seen = set()
    image = bytearray()
    with main_dump.open("rb") as dump:
        if reference is None:
            first = entries[0]
            if first < MAPPED_BLOCK_SIZE or first % MAPPED_BLOCK_SIZE:
                raise ValueError(f"Invalid primary map entry 0 at 0x{first:X}.")
            dump.seek(first)
            header = dump.read(SECTOR_SIZE)
            if len(header) != SECTOR_SIZE or not is_compact_safe_mode(header):
                raise ValueError("The primary table does not resolve to a recognized compact safe-mode ROM.")
            size, = struct.unpack_from("<I", header, 0x80)
        else:
            if len(reference) < SECTOR_SIZE:
                raise ValueError("The original ROM is shorter than its header.")
            size, = struct.unpack_from("<I", reference, 0x80)
            if size > len(reference):
                raise ValueError("The original ROM is shorter than its declared size.")
        if not SECTOR_SIZE <= size <= len(entries) * MAPPED_BLOCK_SIZE:
            raise ValueError("Original ROM size is missing or exceeds the available table entries.")

        for index, offset in enumerate(range(0, size, MAPPED_BLOCK_SIZE)):
            address = entries[index]
            if address < MAPPED_BLOCK_SIZE or address % MAPPED_BLOCK_SIZE or address in seen:
                raise ValueError(f"Invalid primary map entry {index} at 0x{address:X}.")
            seen.add(address)
            length = min(MAPPED_BLOCK_SIZE, size - offset)
            dump.seek(address)
            data = dump.read(length)
            if len(data) != length:
                raise ValueError(f"Main backup ends before the mapped ROM range at 0x{address:X}.")
            if reference is not None and data != reference[offset:offset + length]:
                raise ValueError(f"Original ROM differs from NAND at mapped entry {index} (0x{address:X}).")
            image.extend(data)
    return image, entries[:len(seen)]


def describe_safe_mode(main_dump, spare_dump, original, addresses, patched, pages_per_block):
    if pages_per_block not in (64, 128):
        raise ValueError("Use the erase-buffer page count of 64 or 128 from the DS log.")
    block_bytes = pages_per_block * PAGE_SIZE
    if patched is None:
        blocks = {address // block_bytes for address in addresses}
    else:
        changed_pages = set()
        for offset in range(0, len(original), PAGE_SIZE):
            span = slice(offset, offset + PAGE_SIZE)
            if original[span] != patched[span]:
                address = addresses[offset // MAPPED_BLOCK_SIZE] + offset % MAPPED_BLOCK_SIZE
                changed_pages.add(address // PAGE_SIZE)
        blocks = {page // pages_per_block for page in changed_pages}
    lines = [f"Primary table at 0x{PRIMARY_TABLE:X} maps a 0x{len(original):X}-byte ROM.",
             f"Erase geometry: {pages_per_block} pages per block (0x{block_bytes:X} main bytes)."]
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
    return "\n".join(lines)


def inspect_safe_mode(main_dump, spare_dump, original_path, patched_path, pages_per_block):
    reference = original_path.read_bytes()
    original, addresses = load_primary_rom(main_dump, reference)
    patched = None
    if patched_path is not None:
        candidate = patched_path.read_bytes()
        if len(candidate) != len(reference) or candidate[len(original):] != reference[len(original):]:
            raise ValueError("The patched ROM must preserve the file length and bytes past its declared size.")
        patched = candidate[:len(original)]
    report = describe_safe_mode(main_dump, spare_dump, original, addresses, patched, pages_per_block)
    return report + "\nMapped bytes match the supplied original ROM. No flash image was created."


def inspect_safe_mode_replacement(main_dump, spare_dump, original_path, replacement_path,
                                  pages_per_block):
    reference = original_path.read_bytes()
    replacement = replacement_path.read_bytes()
    if len(replacement) < SECTOR_SIZE or not is_compact_safe_mode(replacement):
        raise ValueError("Replacement needs a compact DS ROM header.")
    if replacement[0x12] or crc16(replacement[:0x15E]) != struct.unpack_from("<H", replacement, 0x15E)[0]:
        raise ValueError("Replacement must be a DS-mode ROM with a valid header CRC.")

    used, = struct.unpack_from("<I", replacement, 0x80)
    if not SECTOR_SIZE <= used <= len(replacement):
        raise ValueError("Replacement declared size is outside the file.")
    for cpu, header_offset in (("ARM9", 0x20), ("ARM7", 0x30)):
        source, _, _, length = struct.unpack_from("<4I", replacement, header_offset)
        if source < SECTOR_SIZE or not length or source + length > used:
            raise ValueError(f"Replacement {cpu} section is outside the declared ROM.")

    original, addresses = load_primary_rom(main_dump, reference)
    capacity = len(addresses) * MAPPED_BLOCK_SIZE
    if used > capacity:
        raise ValueError(f"Replacement needs 0x{used:X} bytes. The original ROM only establishes "
                         f"0x{capacity:X} mapped bytes.")
    if used > len(original):
        # Growth can only use the last block already mapped by the original ROM.
        with main_dump.open("rb") as source:
            source.seek(addresses[-1] + len(original) % MAPPED_BLOCK_SIZE)
            tail = source.read(used - len(original))
        if len(tail) != used - len(original):
            raise ValueError("Main backup ends before the replacement's mapped tail.")
        original.extend(tail)

    report = describe_safe_mode(main_dump, spare_dump, original[:used], addresses,
                                replacement[:used], pages_per_block)
    return (f"Verified mapped capacity from the original ROM: 0x{capacity:X} bytes.\n"
            f"{report}\nRead-only layout check. Bootability and spare format were not verified.")


def prepare_stock_safe_mode(main_dump, spare_dump, pages_per_block):
    original, addresses = load_primary_rom(main_dump)
    patched, (source, end, destination, entry, _) = patch_stock_rom(original)
    report = describe_safe_mode(main_dump, spare_dump, original, addresses, patched, pages_per_block)
    details = (f"ARM7 staging at 0x{STOCK_STAGE_ADDRESS:08X}, ROM slot 0x{source:X}–0x{end - 1:X}.\n"
               f"Restores the ARM7 payload at 0x{destination:08X}, entry 0x{entry:08X}.\n"
               "Prepared a standalone ROM for testing. NAND writes require a separate verified workflow.")
    return patched, report + "\n" + details


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
    safe_mode = parser.add_mutually_exclusive_group()
    safe_mode.add_argument("--backup-info", type=Path, help="check a block-0 backup against its backup.info")
    safe_mode.add_argument("--safe-mode-rom", type=Path, help="original safe-mode ROM from the target cart")
    safe_mode.add_argument("--prepare-safe-mode", type=Path, metavar="OUTPUT",
                           help="extract and patch the stock safe-mode ROM from the main dump")
    candidate = parser.add_mutually_exclusive_group()
    candidate.add_argument("--patched-rom", type=Path, help="compare a patched copy and list only changed erase blocks")
    candidate.add_argument("--replacement-rom", type=Path, help="check a different ROM against the original mapping")
    parser.add_argument("--pages-per-block", type=int, choices=(64, 128), help="erase-buffer page count from the DS log")
    args = parser.parse_args()
    if (args.safe_mode_rom or args.prepare_safe_mode) and args.pages_per_block is None:
        parser.error("safe-mode actions require --pages-per-block from the DS log")
    if (args.patched_rom or args.replacement_rom) and not args.safe_mode_rom:
        parser.error("--patched-rom and --replacement-rom each require --safe-mode-rom")
    if args.pages_per_block and not (args.safe_mode_rom or args.prepare_safe_mode):
        parser.error("--pages-per-block requires a safe-mode action")
    try:
        if args.backup_info:
            report, valid = check_block0_backup(args.main_dump, args.spare_dump, args.backup_info)
            print(report)
            if not valid:
                return 1
        elif args.prepare_safe_mode:
            patched, report = prepare_stock_safe_mode(args.main_dump, args.spare_dump, args.pages_per_block)
            with args.prepare_safe_mode.open("xb") as output:
                output.write(patched)
            print(f"Saved {args.prepare_safe_mode}\n{report}")
        elif args.replacement_rom:
            print(inspect_safe_mode_replacement(args.main_dump, args.spare_dump, args.safe_mode_rom,
                                                args.replacement_rom, args.pages_per_block))
        elif args.safe_mode_rom:
            print(inspect_safe_mode(args.main_dump, args.spare_dump, args.safe_mode_rom,
                                    args.patched_rom, args.pages_per_block))
        else:
            print(inspect_boot(args.main_dump, args.spare_dump))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())
