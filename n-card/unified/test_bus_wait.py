"""Exercise the linked driver's real ROMCTRL polling and failure paths."""
from pathlib import Path
import ctypes as c
import struct
import sys

from test_format_roundtrip import Driver, BUFFER, formatted_nand

CTRL = 0x040001A4
CMD = 0x040001A8
DATA = 0x04100010
BUSY = 1 << 31
READY = 1 << 23


def test(elf, library):
    nand, report, base = formatted_nand(library, 128, 0x40000)
    arm = Driver(elf, nand, library, 0x2500F1EC, base, 128)
    u = arm.cpu
    transfer = 0
    words = 0
    cut_read = cut_write = nand_busy = False
    stuck = True
    commands = []
    stream = []
    io_regs = bytearray(0x1000)

    def io_write(uc, offset, size, value, state):
        nonlocal transfer, words
        if offset != CTRL - 0x04000000:
            io_regs[offset:offset+size] = value.to_bytes(size, "little")
            return
        transfer = value
        start = CMD - 0x04000000
        commands.append(bytes(io_regs[start:start+8]))
        if transfer == 0xa1000000:
            words = 5 if cut_read else 128
        elif transfer == 0xe1000008:
            words = 5 if cut_write else 128
        else:
            words = 1

    def io_read(uc, offset, size, state):
        if offset == CTRL - 0x04000000:
            return BUSY if stuck else (BUSY | READY) if words else 0
        return int.from_bytes(io_regs[offset:offset+size], "little")

    def data_read(uc, offset, size, state):
        nonlocal words
        assert offset == DATA - 0x04100000 and size == 4 and words > 0
        words -= 1
        if transfer == 0xa1000000:
            return 0x12345678
        return 0 if nand_busy and commands[-1][0] == 0xc2 else 0x80808080

    def data_write(uc, offset, size, value, state):
        nonlocal words
        assert offset == DATA - 0x04100000 and size == 4 and words > 0
        stream.append(value)
        words -= 1

    u.mmio_map(0x04000000, 0x1000, io_read, None, io_write, None)
    u.mmio_map(0x04100000, 0x1000, data_read, None, data_write, None)
    assert not arm.call("nrio_wait_card_data", limit=24000000)
    assert arm.word("nrio_bus_fault")
    stuck = False
    assert arm.call("nrio_startup") and not arm.word("nrio_bus_fault")
    u.hook_del(arm.hooks["nrio_read_nand_sector"])
    assert arm.call("nrio_read_sectors", 0, 1, BUFFER)
    assert bytes(u.mem_read(BUFFER, 512)) == struct.pack("<I", 0x12345678) * 128

    cut_read = True
    assert not arm.call("nrio_read_sectors", 1, 1, BUFFER)
    assert arm.word("nrio_bus_fault") and not arm.word("nrio_sector_capacity")
    before = len(commands)
    assert not arm.call("nrio_read_sectors", 1, 1, BUFFER)
    assert not arm.call("nrio_write_sectors", 1, 1, BUFFER)
    assert len(commands) == before
    assert arm.call("nrio_startup") and arm.word("nrio_sector_capacity") == report.sectors

    # Test the real write FIFO, leaving the simulated high-level allocator in
    # place. A short transfer must never get a NAND program-confirm command.
    u.hook_del(arm.hooks["nrio_write_nand_sector_with_ecc"])
    cut_write = True
    commands.clear()
    u.mem_write(BUFFER, b"\x42"*512)
    assert not arm.call("nrio_write_sectors", 0, 1, BUFFER)
    assert arm.word("nrio_bus_fault") and len(stream) == 5
    assert not any(cmd.startswith(b"\xd1\x61\x10") for cmd in commands)
    assert not any(op == "program" for op, page in arm.mutations)

    assert arm.call("nrio_startup")
    cut_write = False
    stream.clear()
    u.hook_del(arm.hooks["nrio_write_nand_sector_raw"])
    payload = bytes(i & 255 for i in range(512))
    u.mem_write(BUFFER, payload)
    arm.call("nrio_set_nand_address", base)
    arm.call("nrio_write_nand_sector_raw", BUFFER, 0)
    assert not arm.word("nrio_bus_fault")
    assert b"".join(struct.pack("<I", w) for w in stream) == payload[1:] + b"\xff"

    nand_busy = True
    commands.clear()
    arm.call("nrio_wait_nand")
    assert arm.word("nrio_bus_fault")
    assert len(commands) == 8192
    print("Slot-1 fault checks passed: stuck busy, short FIFO, no partial commit, and startup retry.")


if __name__ == "__main__":
    test(Path(sys.argv[1]), c.CDLL(str(Path(sys.argv[2]).resolve())))
