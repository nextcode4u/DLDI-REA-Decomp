"""Verify the native formatter against a simulated NAND and the stock FAT writer.

Supply a shared library built from format-native.c, runtime.c and payload.c.
The test does not access physical hardware.
"""
import ctypes as c
from pathlib import Path
from struct import unpack_from
import sys

PAGE = 2048
RECORD = 2112
BLANK = b"\xff" * RECORD
READ = c.CFUNCTYPE(c.c_bool, c.c_void_p, c.c_uint32, c.c_void_p)
ERASE = c.CFUNCTYPE(c.c_bool, c.c_void_p, c.c_uint32)
PROGRAM = c.CFUNCTYPE(c.c_bool, c.c_void_p, c.c_uint32, c.c_void_p)
PROGRESS = c.CFUNCTYPE(None, c.c_void_p, c.c_int, c.c_uint32, c.c_uint32)
FAT_WRITE = c.CFUNCTYPE(c.c_bool, c.c_void_p, c.c_uint32, c.c_uint32, c.c_void_p)


class NandIO(c.Structure):
    _fields_ = [("ctx", c.c_void_p), ("read", READ), ("erase", ERASE),
                ("program", PROGRAM), ("progress", PROGRESS)]


class Report(c.Structure):
    _fields_ = [("stage", c.c_int), ("page", c.c_uint32), ("bad", c.c_uint32),
                ("sectors", c.c_uint32), ("map_address", c.c_uint32),
                ("known_bad", c.c_uint32), ("tested", c.c_uint32), ("bitmap_address", c.c_uint32)]


class FatIO(c.Structure):
    _fields_ = [("ctx", c.c_void_p), ("write", FAT_WRITE)]


def run(lib, pages_per_block, sectors, forbid_map_erase=False, fail_fat=False, mixed_map=False):
    base = 0x1000000 if pages_per_block == 128 else 0x800000
    map_page = (base - 0x100000) // PAGE
    data_page = base // PAGE
    bitmap_page = (base - 0x200000) // PAGE
    stages, erases, programs = [], [], []
    store = {}

    def tag(data, marker):
        return data + b"\x55\xaa\x55\xaa" + b"\xff" * 3 + bytes([marker]) + b"\xff" * 3 + bytes([marker]) + b"\xff" * 3 + bytes([marker]) + b"\xff" * 48

    # Existing stock bitmap marks slot 0 bad. Keep its bytes exactly intact.
    store[bitmap_page] = tag(bytes([1]) + bytes(PAGE - 1), 0xaa)
    store[bitmap_page + 1] = tag(bytes(PAGE), 0xaa)
    before_bitmap = store[bitmap_page]
    # An independent factory marker makes slot 1 unsafe to erase as well.
    store[data_page + pages_per_block] = BLANK[:PAGE] + b"\x00" + BLANK[PAGE + 1:]
    # Simulate an existing valid map ahead of any fallback candidate.
    store[map_page] = tag(bytes(PAGE), 0xab)
    store[0] = b"\x01" * RECORD
    if mixed_map:
        store[map_page + 32] = b"\x01" + BLANK[1:]

    @READ
    def read(_, page, out):
        c.memmove(out, store.get(page, BLANK), RECORD)
        return True

    @ERASE
    def erase(_, page):
        erases.append(page)
        if page % pages_per_block or (page != map_page and page < data_page):
            return False
        if forbid_map_erase and page == map_page:
            return False
        for i in range(page, page + pages_per_block):
            store.pop(i, None)
        return True

    @PROGRAM
    def program(_, page, p):
        programs.append(page)
        if fail_fat and page >= data_page:
            return False
        if page in store or page < map_page:
            return False
        store[page] = c.string_at(p, RECORD)
        return True

    @PROGRESS
    def progress(_, stage, done, total):
        if not stages or stages[-1] != stage:
            stages.append(stage)

    io, report = NandIO(None, read, erase, program, progress), Report()
    ok = lib.udisk_native_quick_format(c.byref(io), sectors, pages_per_block, c.byref(report))
    assert ok == (not forbid_map_erase and not fail_fat and not mixed_map)
    if mixed_map:
        assert report.stage == 0 and not erases and not programs
        return
    assert 0 not in erases and store[0] == b"\x01" * RECORD
    assert data_page not in erases and data_page + pages_per_block not in erases
    assert store[bitmap_page] == before_bitmap and report.bad == 2
    if fail_fat:
        assert report.stage == 3 and map_page not in erases
        return
    if forbid_map_erase:
        assert report.stage == 4 and map_page not in programs
        assert all(p >= data_page for p in programs)
        return
    assert stages == [1, 2, 3, 4, 5]
    assert report.map_address // PAGE == map_page
    assert programs[-32:] == list(range(map_page, map_page + 32))
    assert all(store[page][PAGE + 11] == 0xab for page in programs[-32:])
    table = b"".join(store[page][:PAGE] for page in programs[-32:])
    assert unpack_from(">H", table, 0)[0] << 16 != base

    # Re-read through the same persistent zone/block markers used by DLDI.
    def logical_sector(lba):
        physical_hint = lba + 62 * pages_per_block
        logical, sector = divmod(physical_hint, pages_per_block * 4)
        zone, block = divmod(logical, 62)
        for slot in range(64):
            offset = (zone * 64 + slot) * 2
            p = (unpack_from(">H", table, offset)[0] << 16) // PAGE
            a, b = store.get(p, BLANK), store.get(p + pages_per_block - 1, BLANK)
            if a[PAGE + 11] == b[PAGE + 11] == block and a[PAGE] == b[PAGE] == 0x55:
                record = store.get(p + sector // 4, BLANK)
                col = sector % 4 * 512
                return record[col:col + 512]
        return BLANK[:512]

    expected = {}

    @FAT_WRITE
    def record_sectors(_, lba, count, p):
        data = c.string_at(p, count * 512)
        for i in range(count):
            expected[lba + i] = data[i * 512:(i + 1) * 512]
        return True

    scratch = c.create_string_buffer(65536)
    assert lib.udisk_format_volume(c.byref(FatIO(None, record_sectors)), report.sectors, scratch)
    assert expected[0][3:11] == b"DSGBAFLS"
    assert all(logical_sector(lba) == data for lba, data in expected.items())


def main():
    library = c.CDLL(str(Path(sys.argv[1]).absolute()))
    library.udisk_native_quick_format.argtypes = [c.POINTER(NandIO), c.c_uint32, c.c_uint32, c.POINTER(Report)]
    library.udisk_native_quick_format.restype = c.c_bool
    library.udisk_format_volume.argtypes = [c.POINTER(FatIO), c.c_uint32, c.c_void_p]
    library.udisk_format_volume.restype = c.c_bool
    run(library, 128, 0x400000)
    run(library, 64, 0x200000)
    run(library, 128, 0x200000)
    run(library, 64, 0x40000)
    run(library, 128, 0x100000, mixed_map=True)
    run(library, 128, 0x100000, forbid_map_erase=True)
    run(library, 128, 0x100000, fail_fat=True)
    print("Native format checks passed: bad blocks skipped, boot area unchanged, FAT readback matches, map published last.")


if __name__ == "__main__":
    main()
