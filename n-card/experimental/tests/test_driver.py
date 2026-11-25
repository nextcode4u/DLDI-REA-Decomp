"""Exercise the linked experimental or unified DLDI under Unicorn with simulated I/O."""

import os
from pathlib import Path
import struct
import subprocess
import sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_LR,
    UC_ARM_REG_SP, UC_ARM_REG_PC,
)

elf = Path(sys.argv[1])
image = bytearray(elf.with_suffix(".dldi").read_bytes())
symbols = {
    name: int(address, 16)
    for line in subprocess.check_output(
        [os.getenv("NM", "arm-none-eabi-nm"), str(elf)], text=True
    ).splitlines()
    if len(line.split()) == 3
    for address, _, name in [line.split()]
}
base, _, _, _, got, got_end, _, bss_end = struct.unpack_from("<8I", image, 0x40)
destination = 0x02200000
offset = destination - base
stop, stack, buffer = 0x02300000, 0x02308000, 0x02308100
unified = image[0x64] == 0x23
assert bss_end - base <= 0x8000 and image[0x60:0x64] == b"NRIO"
assert image[0x64:0x68] in (b"\x21\0\0\0", b"\x23\0\0\0")
for i in range(got - base, got_end - base, 4):
    value, = struct.unpack_from("<I", image, i)
    if base <= value < bss_end:
        struct.pack_into("<I", image, i, value + offset)

uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
uc.mem_map(destination, 0x8000)
uc.mem_map(stop, 0x10000)
uc.mem_write(destination, bytes(image))
uc.reg_write(UC_ARM_REG_SP, stack)

def address(name):
    return symbols[name] + offset

def reg(which=UC_ARM_REG_R0):
    return uc.reg_read(which)

def finish(value=0):
    uc.reg_write(UC_ARM_REG_R0, value)
    uc.reg_write(UC_ARM_REG_PC, reg(UC_ARM_REG_LR))

def hook(name, callback):
    pc = address(name)
    uc.hook_add(UC_HOOK_CODE, lambda *_: callback(), begin=pc, end=pc)

def call(name, *args):
    uc.reg_write(UC_ARM_REG_LR, stop)
    for register, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), args):
        uc.reg_write(register, value)
    uc.emu_start(address(name), stop, count=3000000)
    assert reg(UC_ARM_REG_PC) == stop, name
    return reg()

def set_var(name, value, encoding="<I"):
    uc.mem_write(address(name), struct.pack(encoding, value))

def var(name, encoding="<I"):
    return struct.unpack(encoding, uc.mem_read(address(name), struct.calcsize(encoding)))[0]

id_word = 0x2555D7EC
id_ok, map_found, corrupt_map, missing_footer = True, True, False, False
map_scans = 0
id_reads = 0
id_sequence = []
map_copies = 1
first_map = 0x0f00000
corrupt_first = False
alias_map = False
reload_different = False

def read_id():
    global id_reads
    id_reads += 1
    value = id_sequence.pop(0) if id_sequence else id_word
    uc.mem_write(reg(), struct.pack("<I", value or 0))
    finish(int(value is not None and id_ok))

def find_map():
    candidate = var("nrio_block_table_address")
    finish(int(map_found and candidate < first_map + map_copies * 0x40000))

def load_physical():
    global map_scans
    map_scans += 1
    zone = var("nrio_current_zone", "<H")
    ptr = var("nrio_physical_block_table")
    if zone == 511:
        if not missing_footer:
            uc.mem_write(ptr + 63 * 4, struct.pack("<I", 8128))
    else:
        values = list(range(zone * 64, zone * 64 + 64))
        if (corrupt_map or (corrupt_first and var("nrio_block_table_address") == first_map)) and zone == 1:
            values[0] = 8128
        if alias_map and zone == 1:
            values[0] = 1
        if reload_different:
            values[0], values[1] = values[1], values[0]
        uc.mem_write(ptr, struct.pack("<64I", *values))
    finish()

def fail_write():
    raise AssertionError("Experimental read-only DLDI reached a write routine")

reads = []
def read_ftl():
    reads.append((reg(), reg(UC_ARM_REG_R1), reg(UC_ARM_REG_R2)))
    uc.mem_write(reg(UC_ARM_REG_R1), b"\x5a" * reg(UC_ARM_REG_R2))
    finish()

hook("nrio_read_nand_id", read_id)
hook("nrio_find_block_table", find_map)
hook("nrio_load_physical_block_table", load_physical)
hook("nrio_load_zone", finish)
hook("nrio_ftl_read", read_ftl)
if unified:
    write_results = []
    writes = []

    def write_sector():
        ptr = var("nrio_write_buffer")
        assert ptr % 4 == 0
        writes.append(bytes(uc.mem_read(ptr, 512)))
        finish(write_results.pop(0) if write_results else 0)

    hook("nrio_write_sector_with_mapping", write_sector)
else:
    hook("nrio_ftl_write", fail_write)
hook("nrio_retry_block_erase", fail_write)

assert call("nrio_startup") == 0 and map_scans == 0 and id_reads == 4
id_word = 0x2555D5EC
id_ok = False
assert call("nrio_startup") == 0 and map_scans == 0
id_ok, map_found = True, False
assert call("nrio_startup") == 0 and map_scans == 0
map_found, missing_footer = True, True
assert call("nrio_startup") == 0 and map_scans == 1
missing_footer, corrupt_map = False, True
assert call("nrio_startup") == 0 and var("nrio_sector_capacity") == 0
corrupt_map = False
if unified:
    alias_map = True
    assert call("nrio_startup") == 0 and var("nrio_sector_capacity") == 0
    alias_map = False
    assert call("nrio_write_sectors", 0, 1, buffer) == 0 and not writes
corrupt_first, map_copies = True, 2
before = id_reads
id_sequence = [None, 0x2555D7EC, id_word, id_word]
assert call("nrio_startup") == 1 and id_reads == before + 4
assert var("nrio_block_table_address") == first_map + 0x40000
capacity = var("nrio_sector_capacity")
assert 0 < capacity < 0x400000
assert call("nrio_read_sectors", capacity - 1, 1, buffer) == 1
assert reads[-1] == ((capacity - 1) * 512, buffer, 512)
assert call("nrio_read_sectors", capacity - 1, 2, buffer) == 0
assert call("nrio_read_sectors", 0, 1, buffer + 1) == 1
assert uc.mem_read(buffer + 1, 512) == b"\x5a" * 512
if unified:
    assert call("nrio_write_sectors", capacity - 1, 2, buffer) == 0 and not writes
    pattern = bytes(i & 255 for i in range(512))
    uc.mem_write(buffer + 1, pattern)
    assert call("nrio_write_sectors", 0, 1, buffer + 1) == 1 and writes == [pattern]
    write_results = [9]
    assert call("nrio_write_sectors", 0, 2, buffer) == 0 and len(writes) == 2
    write_results = [0x10, 0x10, 0x10]
    assert call("nrio_write_sectors", 0, 1, buffer) == 0 and len(writes) == 5
    set_var("nrio_current_zone", 1, "<H")
    assert call("nrio_reload_validated_table") == 1
    reload_different = True
    assert call("nrio_reload_validated_table") == 0
    assert call("nrio_write_sectors", 0, 1, buffer) == 0 and len(writes) == 5
else:
    assert call("nrio_write_sectors", 0, 1, buffer) == 0
# Existing legacy data can contain a duplicate partially programmed block.
# The read-only build must report it without calling the erase helper.
selected_page = 0
first_marker, last_marker = 1, 0xff

def select_page():
    global selected_page
    selected_page = reg(UC_ARM_REG_R1)
    finish()

def fake_spare():
    marker = first_marker if selected_page == 0 else last_marker
    uc.mem_write(address("nrio_spare") + 11, bytes([marker]))
    finish()

hook("nrio_select_physical_page", select_page)
hook("nrio_page_operation", fake_spare)
hook("nrio_check_spare_markers", finish)
set_var("nrio_pending_physical_block", 3, "<B")
assert call("nrio_classify_physical_block", 4) == 0xfe
first_marker, last_marker = 0xab, 0xab
assert call("nrio_classify_physical_block", 4) == 0xfe

assert call("nrio_shutdown") == 1
assert call("nrio_read_sectors", 0, 1, buffer) == 0
print("Relocated DLDI checks passed:", "unified read/write" if unified else "experimental read-only")
