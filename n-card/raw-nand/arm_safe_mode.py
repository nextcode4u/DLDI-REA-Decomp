"""Prepare a cart-specific stock safe-mode installer from physical NAND backups."""

import argparse
import struct
from pathlib import Path

from arm_banner import default_template
from inspect_boot import MAPPED_BLOCK_SIZE, PAGE_SIZE, SECTOR_SIZE, SPARE_SIZE, describe_safe_mode, load_primary_rom
from patch_safe_mode import patch_stock_rom, read_word

SIGNATURE = b"NRIO-SAFE-v1".ljust(16, b"\0")
INSTALLER_ROM = "nrio-stock-safe-mode-installer.nds"
WORKSPACE_BYTES = 128 * (PAGE_SIZE + SPARE_SIZE)


def prepare_installer(main_dump, spare_dump, pages, template):
    if pages not in (64, 128):
        raise ValueError("Use the erase-buffer page count of 64 or 128 from the DS log.")
    main_size, spare_size = pages * PAGE_SIZE, pages * SPARE_SIZE
    with main_dump.open("rb") as source:
        main = source.read(main_size)
    with spare_dump.open("rb") as source:
        spare = source.read(spare_size)
    if len(main) != main_size or len(spare) != spare_size:
        raise ValueError(f"Block 0 requires 0x{main_size:X} main and 0x{spare_size:X} packed spare bytes.")

    original, addresses = load_primary_rom(main_dump)
    patched, (arm7_source, _, _, _, _) = patch_stock_rom(original)
    changed = [offset for offset in range(0, len(original), SECTOR_SIZE)
               if original[offset:offset + SECTOR_SIZE] != patched[offset:offset + SECTOR_SIZE]]
    if changed != [0, arm7_source]:
        raise ValueError("The stock patch must change just the ROM header and ARM7 sectors.")

    sectors = []
    for offset in changed:
        address = addresses[offset // MAPPED_BLOCK_SIZE] + offset % MAPPED_BLOCK_SIZE
        if not 0x18000 <= address <= main_size - SECTOR_SIZE:
            raise ValueError("This installer supports only safe-mode sectors in block 0 past the primary table.")
        if main[address:address + SECTOR_SIZE] != original[offset:offset + SECTOR_SIZE]:
            raise ValueError("The main backup changed during preparation.")
        sectors.append(address // SECTOR_SIZE)
    if read_word(main, 0x8000) != addresses[0] or read_word(main, 0x8000 + arm7_source // MAPPED_BLOCK_SIZE * 4) != addresses[arm7_source // MAPPED_BLOCK_SIZE]:
        raise ValueError("The primary table changed during preparation.")

    records = bytearray()
    for page in range(pages):
        records += main[page * PAGE_SIZE:(page + 1) * PAGE_SIZE]
        records += spare[page * SPARE_SIZE:(page + 1) * SPARE_SIZE]
    plan = (struct.pack("<III", pages, *sectors) + records.ljust(WORKSPACE_BYTES, b"\0")
            + b"".join(patched[offset:offset + SECTOR_SIZE] for offset in changed))
    image = template.read_bytes()
    offset = image.find(SIGNATURE)
    if offset < 0 or image.find(SIGNATURE, offset + 1) >= 0:
        raise ValueError("Use an unmodified toolkit build that supports the stock safe-mode installer.")
    start = offset + len(SIGNATURE)
    end = start + len(plan)
    if image[start:end] != bytes(len(plan)):
        raise ValueError("The toolkit template already contains a safe-mode install plan.")
    report = describe_safe_mode(main_dump, spare_dump, original, addresses, patched, pages)
    report += f"\nPrepared sectors 0x{sectors[0]:X} and 0x{sectors[1]:X} against the full block-0 backup."
    return image[:start] + plan + image[end:], report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target_main", type=Path)
    parser.add_argument("target_spare", type=Path)
    parser.add_argument("--pages-per-block", type=int, required=True, choices=(64, 128))
    parser.add_argument("--template", type=Path, default=default_template())
    parser.add_argument("--output", type=Path, default=Path(INSTALLER_ROM))
    args = parser.parse_args()
    try:
        image, report = prepare_installer(args.target_main, args.target_spare, args.pages_per_block, args.template)
        with args.output.open("xb") as output:
            output.write(image)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Saved {args.output}\n{report}")


if __name__ == "__main__":
    main()
