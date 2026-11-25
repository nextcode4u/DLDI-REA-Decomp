"""Test destructive Full Format against fault-injected simulated NAND."""
import ctypes as c
from collections import Counter
from pathlib import Path
from struct import unpack_from
import sys

from test_native_format import BLANK, ERASE, NandIO, PAGE, PROGRAM, PROGRESS, READ, RECORD, Report


def run(lib, pages, fault=None):
    base = 0x1000000 if pages == 128 else 0x800000
    first = base // PAGE
    bitmap = (base - 0x200000) // PAGE
    table = (base - 0x100000) // PAGE
    blocks = 80
    boot = b"boot" + BLANK[4:]
    store = {0: boot}
    erase_count, scratch, events = Counter(), Counter(), []
    known = {0, 1, 2}
    program_bad, misread, erase_bad = (first + b * pages for b in (3, 4, 5))
    first_mixed_page = bitmap + 2
    tags = b"\x55\xaa\x55\xaa" + (b"\xff" * 3 + b"\xaa") * 3
    for marker_copy, bit in ((bitmap, 1), (bitmap + pages, 2)):
        store[marker_copy] = bytes([bit]) + bytes(PAGE-1) + tags + b"\xff"*48
        store[marker_copy+1] = bytes(PAGE) + tags + b"\xff"*48
    store[first + 2 * pages] = BLANK[:PAGE] + b"\0" + BLANK[PAGE+1:]
    if fault == "mixed bitmap":
        store[first_mixed_page] = b"\0" + BLANK[1:]

    @READ
    def read(_, p, out):
        if fault == "lost read" and p == misread + 3 and p in store:
            return False
        data = store.get(p, BLANK)
        if p == misread + 3 and p in store and erase_count[misread] == 1:
            data = bytes([data[0] ^ 1]) + data[1:]
        c.memmove(out, data, RECORD)
        return True

    @ERASE
    def erase(_, p):
        assert p in (bitmap, table) or first <= p < first + blocks * pages
        assert p % pages == 0
        events.append(("erase", p))
        erase_count[p] += 1
        if p == erase_bad and erase_count[p] == 2:
            return False
        for i in range(p, p+pages):
            store.pop(i, None)
        return True

    @PROGRAM
    def program(_, p, data):
        assert p in (bitmap, bitmap+1) or table <= p < table + 32 or first <= p < first + blocks*pages
        record = c.string_at(data, RECORD)
        assert p not in store
        if p >= first and record[PAGE+11] == 0xff:
            block = p - (p-first) % pages
            scratch[block] += 1
            assert record[PAGE:PAGE+16] == b"\xff"*16
            assert record[:PAGE] != BLANK[:PAGE]
            if p == program_bad + 7:
                return False
        store[p] = record
        events.append(("program", p))
        return True

    stages = []

    @PROGRESS
    def progress(_, stage, done, total):
        if not stages or stages[-1] != stage:
            stages.append(stage)

    io = NandIO(None, read, erase, program, progress)
    report = Report()
    result = lib.udisk_native_full_format(c.byref(io), first*4 + blocks*pages*4,
                                          pages, c.byref(report))
    assert store[0] == boot
    if fault == "mixed bitmap":
        assert not result and not events and report.stage == 0
        return
    if fault == "lost read":
        assert not result and report.stage == 6 and report.page == misread + 3
        assert not erase_count[table] and not erase_count[bitmap]
        return
    assert result and stages == [1, 6, 3, 7, 4, 5]
    assert report.known_bad == 3 and report.tested == 77 and report.bad == 6
    assert report.bitmap_address == bitmap*PAGE and report.map_address == table*PAGE
    assert store[bitmap][0] == 0x3f and store[bitmap+1][PAGE+11] == 0xaa
    assert events[-34:] == [("program", bitmap+1), ("erase", table)] + [
        ("program", table+i) for i in range(32)]
    raw_table = b"".join(store[table+i][:PAGE] for i in range(32))
    mapped = {((unpack_from(">H", raw_table, i * 2)[0] << 16)//PAGE - first)//pages
              for i in range(64)}
    assert len(mapped) == 64 and not mapped.intersection(range(6))
    for i in range(blocks):
        p = first + i*pages
        if i in known:
            assert not erase_count[p] and not scratch[p]
        elif i >= 6:
            assert erase_count[p] == 2 and scratch[p] == pages


if __name__ == "__main__":
    library = c.CDLL(str(Path(sys.argv[1]).resolve()))
    library.udisk_native_full_format.argtypes = [c.POINTER(NandIO), c.c_uint32, c.c_uint32, c.POINTER(Report)]
    library.udisk_native_full_format.restype = c.c_bool
    run(library, 64)
    run(library, 128)
    run(library, 64, "lost read")
    run(library, 128, "mixed bitmap")
    print("Full Format tests passed for both geometries, bad-block persistence, boot protection and I/O abort.")
