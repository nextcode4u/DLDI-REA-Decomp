"""Trace stock uDisk 1.45 Quick Format setup with simulated NAND reads.

Run with Unicorn installed and the archived nand_stable_25/udisk.nds as input.
This stops at the first erase or test and never accesses a physical device.
"""

import hashlib
from pathlib import Path
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
    UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
)

ARM9 = 0x02000000
STOP = 0x023F0000
ID = bytes.fromhex("ec d5 55 25")
STOCK_SHA256 = "9c83cfa1bbfb9b928ccabc4403144480ba1428a80d069908f76fd3d56a49c686"


def trace(image, mode, have_bitmap):
    machine = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    machine.mem_map(ARM9, 0x400000)
    machine.mem_map(0x0B000000, 0x200000)  # Scratch RAM used by the stock app.
    machine.mem_write(ARM9, image)
    machine.reg_write(UC_ARM_REG_SP, STOP - 0x1000)
    machine.reg_write(UC_ARM_REG_LR, STOP | 1)
    machine.reg_write(UC_ARM_REG_R0, mode)

    events = []
    bitmap_pointer = None
    selected = None

    def hook(address, fn):
        machine.hook_add(UC_HOOK_CODE, lambda _m, _a, _s, _d: fn(),
                         begin=address, end=address)

    def reg(register):
        return machine.reg_read(register)

    def ret():
        machine.reg_write(UC_ARM_REG_PC, reg(UC_ARM_REG_LR))

    def read_id():
        events.append(("read-id",))
        machine.mem_write(reg(UC_ARM_REG_R0), ID)
        ret()

    def geometry():
        events.append(("geometry", *(reg(r) for r in
                       (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3))))

    def bitmap():
        nonlocal bitmap_pointer
        bitmap_pointer = reg(UC_ARM_REG_R2)
        events.append(("bitmap-read", reg(UC_ARM_REG_R0), reg(UC_ARM_REG_R1)))
        # A known bad first block if a readable bitmap is present.
        machine.mem_write(bitmap_pointer, b"\x01" + b"\x00" * 0xfff if have_bitmap else b"\xff" * 0x1000)
        machine.reg_write(UC_ARM_REG_R0, int(have_bitmap))
        ret()

    def select_address():
        nonlocal selected
        selected = reg(UC_ARM_REG_R0)
        ret()

    def nand_op(name):
        events.append((name, reg(UC_ARM_REG_R0), selected, machine.mem_read(bitmap_pointer, 1)[0]))
        machine.emu_stop()

    hook(0x02008358, read_id)
    hook(0x0200A7BC, geometry)
    hook(0x02009C38, bitmap)
    hook(0x020092C0, select_address)  # Set NAND address only.
    hook(0x020094E4, lambda: nand_op("erase-command"))
    hook(0x020096A4, lambda: nand_op("full-test"))
    for address in (0x0202A80C, 0x0202A814, 0x0202A81C):
        hook(address, ret)  # Console output and progress math.
    machine.emu_start(0x0200AEC1, STOP, count=450000)
    return events


def main():
    rom = Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(rom).hexdigest() != STOCK_SHA256:
        raise SystemExit("Expected the archived 768 KiB stock uDisk 1.45 ROM.")
    offset, _, load, size = struct.unpack_from("<4I", rom, 0x20)
    assert load == ARM9
    image = rom[offset:offset + size]
    expected = [("read-id",), ("read-id",), ("geometry", 0x40000, 8128, 0x1000000, 1),
                ("bitmap-read", 0x1000000, 0x40000)]
    assert trace(image, 1, True) == expected + [("erase-command", 7, 0x1040000, 1)]
    assert trace(image, 1, False) == expected + [("erase-command", 7, 0x1000000, 0)]
    assert trace(image, 2, True)[-1] == ("full-test", 0x1040000, None, 1)
    print("Stock Quick Format reads the bitmap and enters erase, not Full Format block testing.")
    print("For NAND ID 2555D5EC, the data region begins at 0x1000000 with 0x40000-byte erase blocks.")


if __name__ == "__main__":
    main()
