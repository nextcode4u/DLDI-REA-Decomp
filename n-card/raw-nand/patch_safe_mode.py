#!/usr/bin/env python3
"""Prepare a separate DS Fire Card safe-mode ROM for the ARM7 relocation test."""

import argparse
import struct
from pathlib import Path


PAYLOAD_SIZE = 0x38
HANDOFF_SIZE = 8
STOCK_STAGE_ADDRESS = 0x02380000
STOCK_ARM7_ADDRESS = 0x03800000
STOCK_ARM7_SLOT_SIZE = 0x200
MAIN_RAM = (0x02000000, 0x023BFE00)
ARM7_WRAM = (0x037F8000, 0x03807E00)

# ARM7TDMI, position-independent. Scratch words live in the staging copy.
# Save r0-r3 and ip, save flags, then copy 14 words from the inline payload.
# Restore flags and registers and load the original ARM entry into pc.
STUB = (
    0xE58F0050, 0xE58F1050, 0xE58F2050, 0xE58F3050, 0xE58FC050,  # str to scratch words
    0xE10F3000,  # mrs r3, cpsr
    0xE28F004C, 0xE59F102C, 0xE3A0200E,  # adr r0, payload; ldr r1, destination; mov r2, #14
    0xE490C004, 0xE481C004, 0xE2522001, 0x1AFFFFFB,  # ldr/str post-index; subs; bne
    0xE128F003,  # msr cpsr_f, r3
    0xE59F0018, 0xE59F1018, 0xE59F2018, 0xE59F3018, 0xE59FC018,  # ldr from scratch words
    0xE59FF000,  # ldr pc, entry
)


def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def overlaps(start, end, other_start, other_end):
    return start < other_end and other_start < end


def read_word(rom, offset):
    return struct.unpack_from("<I", rom, offset)[0]


def occupied_sections(rom):
    # Section offsets and lengths in the standard NTR header.
    for label, offset, size in (
        ("ARM9", 0x20, 0x2C), ("FNT", 0x40, 0x44), ("FAT", 0x48, 0x4C),
        ("ARM9 overlays", 0x50, 0x54), ("ARM7 overlays", 0x58, 0x5C),
        ("debug ROM", 0x160, 0x164),
    ):
        start, length = read_word(rom, offset), read_word(rom, size)
        if length:
            yield label, start, start + length

    fat, size = struct.unpack_from("<II", rom, 0x48)
    if size:
        if size % 8 or fat < 0x200 or fat + size > len(rom):
            raise ValueError("Invalid file allocation table.")
        for start, end in struct.iter_unpack("<II", memoryview(rom)[fat:fat + size]):
            if start > end or end > len(rom):
                raise ValueError("Invalid NitroFS file range.")
            if start < end:
                yield "NitroFS file", start, end

    banner = read_word(rom, 0x68)
    if banner:
        if banner + 2 > len(rom):
            raise ValueError("Invalid banner offset.")
        version, = struct.unpack_from("<H", rom, banner)
        lengths = {1: 0x840, 2: 0x940, 3: 0xA40, 0x103: 0x23C0}
        if version not in lengths:
            raise ValueError(f"Unknown banner version 0x{version:X}.")
        yield "banner", banner, banner + lengths[version]


def is_compact_safe_mode(header):
    return (header[:4] == b"\x2e\x00\x00\xea"
            and header[0x0C:0x10] == b"####"
            and read_word(header, 0x84) == 0x200)


def patch_rom(rom, stage_address):
    if len(rom) < 0x200 or rom[0x12] != 0:
        raise ValueError("Expected a standalone NTR-format safe-mode ROM.")
    if crc16(rom[:0x15E]) != struct.unpack_from("<H", rom, 0x15E)[0]:
        raise ValueError("ROM header CRC does not match. Check the extracted input.")
    source, entry, destination, size = struct.unpack_from("<4I", rom, 0x30)
    if size != PAYLOAD_SIZE:
        raise ValueError(f"Expected a 0x{PAYLOAD_SIZE:X}-byte ARM7 payload, found 0x{size:X}.")
    if (entry % 4 or destination % 4 or not destination <= entry < destination + size or
            not ARM7_WRAM[0] <= destination <= ARM7_WRAM[1] - size - HANDOFF_SIZE):
        raise ValueError("Expected an aligned ARM entry and payload in ARM7 WRAM.")
    used_size = read_word(rom, 0x80)
    compact = is_compact_safe_mode(rom)
    # This loader reads the compact image in sectors. For standard NTR ROMs,
    # keep the replacement outside the secure area.
    alignment = STOCK_ARM7_SLOT_SIZE if compact else 4
    first_source = STOCK_ARM7_SLOT_SIZE if compact else 0x8000
    if source % alignment or not first_source <= source < used_size <= len(rom):
        raise ValueError("ARM7 source must be aligned and inside the declared ROM.")

    payload = rom[source:source + size]
    loader = struct.pack("<" + "I" * (len(STUB) + 7), *STUB, destination, entry, 0, 0, 0, 0, 0)
    replacement = loader + payload
    load_size = STOCK_ARM7_SLOT_SIZE if compact else len(replacement)
    end = source + load_size
    if end > used_size:
        raise ValueError("The stub and payload do not fit inside the declared ROM.")
    padding = rom[source + size:end]
    if padding != bytes(len(padding)) and padding != b"\xff" * len(padding):
        raise ValueError("The ARM7 payload is followed by data. A larger verified slot is needed.")
    for label, start, stop in occupied_sections(rom):
        if overlaps(source, end, start, stop):
            raise ValueError(f"ARM7 replacement would overlap {label} in the ROM.")

    stage_end = stage_address + load_size
    if stage_address % 4 or not MAIN_RAM[0] <= stage_address < stage_end <= MAIN_RAM[1]:
        raise ValueError("Choose a word-aligned staging range in NTR main RAM.")
    arm9_ram, arm9_size = struct.unpack_from("<II", rom, 0x28)
    if not arm9_size or not MAIN_RAM[0] <= arm9_ram < arm9_ram + arm9_size <= MAIN_RAM[1]:
        raise ValueError("ARM9 load range is outside the expected NTR main RAM.")
    if overlaps(stage_address, stage_end, arm9_ram, arm9_ram + arm9_size):
        raise ValueError("ARM7 staging range overlaps the ARM9 image in RAM.")

    patched = bytearray(rom)
    patched[source:source + len(replacement)] = replacement
    struct.pack_into("<III", patched, 0x34, stage_address, stage_address, load_size)
    struct.pack_into("<H", patched, 0x15E, crc16(patched[:0x15E]))
    return patched, (source, end, destination, entry, stage_end)


def patch_stock_rom(rom):
    if len(rom) < 0x200 or not is_compact_safe_mode(rom):
        raise ValueError("The primary table does not resolve to a recognized compact safe-mode ROM.")
    if read_word(rom, 0x38) != STOCK_ARM7_ADDRESS:
        raise ValueError("Expected the unmodified ARM7 payload at 0x03800000.")
    return patch_rom(rom, STOCK_STAGE_ADDRESS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="extracted, unmodified safe-mode .nds ROM")
    parser.add_argument("output", type=Path, help="new .nds filename")
    parser.add_argument("--stage-address", required=True, type=lambda value: int(value, 0),
                        help="confirmed free ARM7 staging address in main RAM")
    args = parser.parse_args()
    try:
        patched, (source, end, destination, entry, stage_end) = patch_rom(
            args.input.read_bytes(), args.stage_address)
        with args.output.open("xb") as output:
            output.write(patched)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Created {args.output} (same ROM size).")
    print(f"ROM ARM7 slot: 0x{source:X}–0x{end - 1:X}")
    print(f"ARM7 staging RAM: 0x{args.stage_address:08X}–0x{stage_end - 1:08X}")
    print(f"Copies 0x{PAYLOAD_SIZE:X} bytes to 0x{destination:08X}, then jumps to 0x{entry:08X}.")


if __name__ == "__main__":
    main()
