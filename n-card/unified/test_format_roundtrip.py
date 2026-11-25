"""Round trip native-format NAND through the relocated, linked ARM DLDI.

Hooks are at the NAND I/O boundary. Map parsing, ECC correction, FTL reads
and writes, block allocation, and remount use the real driver.
"""
from collections import defaultdict
import ctypes as c
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_LR,
    UC_ARM_REG_SP, UC_ARM_REG_PC,
)

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "udisk"))
from test_native_format import PAGE, RECORD, BLANK, READ, ERASE, PROGRAM, PROGRESS, NandIO, Report

SECTOR = 512
BASE = 0x02200000
STOP = 0x02300000
STACK = 0x02308000
BUFFER = 0x02310000


class Driver:
    def __init__(self, elf, nand, library, nand_id, data_base, erase_pages):
        self.nand, self.library = nand, library
        self.data_base, self.erase_pages, self.nand_id = data_base, erase_pages, nand_id
        image = bytearray(elf.with_suffix(".dldi").read_bytes())
        symbols = subprocess.check_output([os.getenv("NM", "arm-none-eabi-nm"), str(elf)], text=True)
        self.symbols = {line.split()[2]: int(line.split()[0], 16)
                        for line in symbols.splitlines() if len(line.split()) == 3}
        base, _, _, _, got, got_end, _, bss_end = struct.unpack_from("<8I", image, 0x40)
        assert image[0x60:0x68] == b"NRIO\x23\x00\x00\x00" and bss_end - base <= 0x8000
        self.delta = BASE - base
        for at in range(got - base, got_end - base, 4):
            ptr, = struct.unpack_from("<I", image, at)
            if base <= ptr < bss_end:
                struct.pack_into("<I", image, at, ptr + self.delta)
        self.cpu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.cpu.mem_map(BASE, 0x8000)
        self.cpu.mem_map(STOP, 0x30000)
        self.cpu.mem_write(BASE, bytes(image))
        self.cpu.reg_write(UC_ARM_REG_SP, STACK)
        self.pending = defaultdict(dict)
        self.status = 0xc0
        self.mutations = []
        self.hooks = {}
        self.hook("nrio_read_nand_id", self.read_id)
        self.hook("nrio_read_nand_spare", self.read_spare)
        self.hook("nrio_read_nand_sector", self.read_sector)
        self.hook("nrio_read_nand_sector_with_ecc", lambda: self.read_sector(ecc=True))
        self.hook("nrio_write_nand_sector_raw", self.stage_sector)
        self.hook("nrio_write_nand_sector_with_ecc", lambda: self.stage_sector(ecc=True))
        self.hook("nrio_finish_nand_write", self.program)
        self.hook("nrio_erase_nand_block", self.erase)
        self.hook("nrio_read_status", lambda: self.ret(self.status))

    def addr(self, name):
        return self.symbols[name] + self.delta

    def hook(self, name, fn):
        loc = self.addr(name)
        self.hooks[name] = self.cpu.hook_add(UC_HOOK_CODE, lambda *_: fn(), begin=loc, end=loc)

    def arg(self, i):
        return self.cpu.reg_read((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2)[i])

    def ret(self, value=0):
        self.cpu.reg_write(UC_ARM_REG_R0, value)
        self.cpu.reg_write(UC_ARM_REG_PC, self.cpu.reg_read(UC_ARM_REG_LR))

    def call(self, name, *args, limit=8000000):
        self.cpu.reg_write(UC_ARM_REG_LR, STOP)
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), args):
            self.cpu.reg_write(reg, value)
        self.cpu.emu_start(self.addr(name), STOP, count=limit)
        assert self.cpu.reg_read(UC_ARM_REG_PC) == STOP, f"{name} did not return"
        return self.arg(0)

    def word(self, name):
        return struct.unpack("<I", self.cpu.mem_read(self.addr(name), 4))[0]

    def row(self):
        return self.word("nrio_nand_address") // PAGE

    def ecc(self, data):
        out = c.create_string_buffer(3)
        self.library.udisk_payload_ecc(data, out)
        return out.raw

    def read_id(self):
        self.cpu.mem_write(self.arg(0), struct.pack("<I", self.nand_id))
        self.ret(1)

    def read_spare(self):
        self.cpu.mem_write(self.arg(0), self.nand.get(self.row(), BLANK)[PAGE:])
        self.ret()

    def read_sector(self, ecc=False):
        start = self.arg(1)*SECTOR
        data = self.nand.get(self.row(), BLANK)[start:start+SECTOR]
        self.cpu.mem_write(self.arg(0), data)
        if ecc:
            self.cpu.mem_write(self.arg(2), self.ecc(data))
        self.ret()

    def stage_sector(self, ecc=False):
        assert self.arg(0) % 4 == 0
        # For user input routed through the ECC writer, final or unaligned
        # sectors must be copied to an aligned buffer with safe lookahead.
        if ecc and STOP <= self.arg(0) < STACK:
            assert bytes(self.cpu.mem_read(self.arg(0) + SECTOR, 8)) == b"\xff" * 8
        data = bytes(self.cpu.mem_read(self.arg(0), SECTOR))
        self.pending[self.row()][self.arg(1)] = data
        if ecc:
            self.cpu.mem_write(self.arg(2), self.ecc(data))
        self.ret()

    def program(self):
        page = self.row()
        assert page >= self.data_base // PAGE, f"FTL tried to program protected page {page:X}"
        old = self.nand.get(page, BLANK)
        record = bytearray(old)
        for column, sector in self.pending.pop(page, {}).items():
            start = column * SECTOR
            record[start:start+SECTOR] = sector
        record[PAGE:] = self.cpu.mem_read(self.arg(0), RECORD-PAGE)
        if any((a & b) != b for a, b in zip(old, record)):
            self.status = 0xc1
        else:
            self.nand[page] = bytes(record)
            self.mutations.append(("program", page))
            self.status = 0xc0
        self.ret()

    def erase(self):
        page = self.row()
        assert page >= self.data_base // PAGE and page % self.erase_pages == 0, hex(page)
        for i in range(page, page + self.erase_pages):
            self.nand.pop(i, None)
            self.pending.pop(i, None)
        self.mutations.append(("erase", page))
        self.status = 0xc0
        self.ret()

    def read(self, first, count, alignment=0):
        assert 0 < count <= 128
        assert self.call("nrio_read_sectors", first, count, BUFFER + alignment)
        return bytes(self.cpu.mem_read(BUFFER + alignment, count*512))

    def write(self, first, data, alignment=0):
        assert len(data) % 512 == 0
        self.cpu.mem_write(BUFFER + alignment, data)
        assert self.call("nrio_write_sectors", first, len(data)//512, BUFFER + alignment)


def restore_sector0(elf, lib, nand, nand_id, base, pages, original):
    """Run the actual restore transaction against the real relocated FTL."""
    active = Driver(elf, nand, lib, nand_id, base, pages)
    assert active.call("nrio_startup")
    corrupt = original[:510] + b"\0\0"
    active.write(0, corrupt)
    assert active.read(0, 1) == corrupt
    saved = []
    RW = c.CFUNCTYPE(c.c_bool, c.c_void_p, c.c_void_p)
    ACTION = c.CFUNCTYPE(c.c_bool, c.c_void_p)

    class Sector0IO(c.Structure):
        _fields_ = [("context", c.c_void_p), ("read", RW), ("write", RW),
                    ("restart", ACTION), ("backup", RW), ("confirm", ACTION)]

    @RW
    def read(_, dest):
        c.memmove(dest, active.read(0, 1), SECTOR)
        return True

    @RW
    def write(_, src):
        active.write(0, c.string_at(src, SECTOR))
        return True

    @ACTION
    def restart(_):
        nonlocal active
        if not active.call("nrio_shutdown"):
            return False
        active = Driver(elf, nand, lib, nand_id, base, pages)
        return bool(active.call("nrio_startup"))

    @RW
    def backup(_, src):
        saved.append(c.string_at(src, SECTOR))
        return True

    @ACTION
    def confirm(_):
        return True

    io = Sector0IO(None, read, write, restart, backup, confirm)
    lib.nrio_sector0_restore.argtypes = [c.POINTER(Sector0IO), c.c_char_p]
    lib.nrio_sector0_restore.restype = c.c_int
    assert lib.nrio_sector0_restore(c.byref(io), original) == 0
    assert saved == [corrupt] and active.read(0, 1) == original
    return active

def formatted_nand(lib, pages, sectors, full=False):
    base = 0x1000000 if pages == 128 else 0x800000
    nand = {0: b"boot" + BLANK[4:]}
    tags = b"\x55\xaa\x55\xaa" + (b"\xff"*3 + b"\xaa")*3 + b"\xff"*48
    bitmap = (base - 0x200000) // PAGE
    nand[bitmap] = b"\1" + bytes(PAGE-1) + tags
    nand[bitmap+1] = bytes(PAGE) + tags

    @READ
    def read(_, page, out):
        c.memmove(out, nand.get(page, BLANK), RECORD)
        return True

    @ERASE
    def erase(_, page):
        for i in range(page, page + pages):
            nand.pop(i, None)
        return True

    @PROGRAM
    def program(_, page, data):
        nand[page] = c.string_at(data, RECORD)
        return True

    null_progress = PROGRESS()
    io = NandIO(None, read, erase, program, null_progress)
    report = Report()
    fn = lib.udisk_native_full_format if full else lib.udisk_native_quick_format
    fn.argtypes = [c.POINTER(NandIO), c.c_uint32, c.c_uint32, c.POINTER(Report)]
    fn.restype = c.c_bool
    assert fn(c.byref(io), sectors, pages, c.byref(report))
    assert report.bad == 1
    return nand, report, base


def roundtrip(elf, lib, pages, full):
    nand_id = 0x2500F1EC if pages == 128 else 0x0500F1EC
    nand, report, base = formatted_nand(lib, pages, 0x40000, full)
    upper = 2048
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        image = root / "fat.img"
        src = root / "roundtrip.bin"
        dst = root / "readback.bin"
        data = bytes((i*13 + (i >> 8)) & 255 for i in range(262157))
        src.write_bytes(data)
        dldi = Driver(elf, nand, lib, nand_id, base, pages)
        assert dldi.call("nrio_startup")
        assert dldi.word("nrio_sector_capacity") == report.sectors
        assert dldi.read(0, 1, alignment=1)[3:11] == b"DSGBAFLS"
        before = b"".join(dldi.read(i, 128) for i in range(0, upper, 128))
        assert not dldi.mutations
        if hasattr(lib, "nrio_sector0_restore"):
            dldi = restore_sector0(elf, lib, nand, nand_id, base, pages, before[:SECTOR])
        with image.open("wb") as output:
            output.write(before)
            output.truncate(report.sectors * SECTOR)
        subprocess.run(["mcopy", "-i", image, src, "::ROUNDTRP.BIN"], check=True)
        with image.open("rb") as disk:
            after = disk.read(len(before))
        changed = [i for i in range(upper)
                   if before[i*SECTOR:(i+1)*SECTOR] != after[i*SECTOR:(i+1)*SECTOR]]
        assert changed
        j = 0
        write_request = 0
        while j < len(changed):
            first = changed[j]
            j += 1
            last = first + 1
            while j < len(changed) and changed[j] == last and last - first < 32:
                j += 1
                last += 1
            dldi.write(first, after[first*SECTOR:last*SECTOR], alignment=write_request % 4)
            write_request += 1
            assert dldi.read(first, last-first) == after[first*SECTOR:last*SECTOR], (first,last)
        assert dldi.mutations
        # Exercise the nonzero-zone caches and final capacity boundary
        # outside the file's allocated clusters on this disposable image.
        crossing = 62 * pages * 3 - 2
        far = report.sectors - 1
        probe = src.read_bytes()[:4*SECTOR]
        dldi.write(crossing, probe, alignment=3)
        dldi.write(far, probe[:SECTOR], alignment=2)
        assert dldi.read(crossing, 4) == probe
        assert dldi.read(far, 1) == probe[:SECTOR]
        assert not dldi.call("nrio_write_sectors", far, 2, BUFFER)
        assert dldi.call("nrio_shutdown")

        fresh = Driver(elf, nand, lib, nand_id, base, pages)
        assert fresh.call("nrio_startup")
        back = b"".join(fresh.read(i, 128) for i in range(0, upper, 128))
        assert back == after
        assert fresh.read(crossing, 4, alignment=1) == probe
        assert fresh.read(far, 1) == probe[:SECTOR]
        assert not fresh.mutations
        with image.open("r+b") as output:
            output.write(back)
        subprocess.run(["mcopy", "-i", image, "::ROUNDTRP.BIN", dst], check=True)
        assert dst.read_bytes() == data
        assert nand[0] == b"boot" + BLANK[4:]
    restored = ", sector 0 restored" if hasattr(lib, "nrio_sector0_restore") else ""
    print(f"{'Full' if full else 'Quick'} Format, {pages} pages/block: file survived write and remount{restored}")


if __name__ == "__main__":
    elf = Path(sys.argv[1])
    library = c.CDLL(str(Path(sys.argv[2]).resolve()))
    library.udisk_payload_ecc.argtypes = [c.c_char_p, c.c_void_p]
    library.udisk_payload_ecc.restype = None
    for format_full in (False, True):
        for geometry in (64, 128):
            roundtrip(elf, library, geometry, format_full)
