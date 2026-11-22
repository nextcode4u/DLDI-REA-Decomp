"""Run the raw DLDI and the linked recovery code against a simulated Slot-1 bridge."""

import argparse
import struct
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_MODE_ARM
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP,
)

from arm_banner import SIGNATURE, arm
from arm_safe_mode import SIGNATURE as SAFE_MODE_SIGNATURE

CARD = 0x04000000
CTRL = CARD + 0x1a4
COMMAND = CARD + 0x1a8
FIFO = 0x04100010
RAM = 0x02100000
STOP = RAM + 0xf0000
PAGE_BYTES = 2112
MAIN_READ, SPARE_READ, PROTECTED, MAIN_WRITE, SPARE_WRITE, BANNER, TABLE_REPAIR, SAFE_MODE = range(8)


class Image:
    def __init__(self, regions, entries):
        self.regions = regions
        self.entries = entries

    def armed(self, expected, main, spare):
        assert sum(data.count(SIGNATURE) for _, _, data in self.regions) == 1
        return Image([(start, size, arm(data, expected, main, spare) if SIGNATURE in data else data)
                      for start, size, data in self.regions], self.entries)


def load_dldi(path):
    data = path.read_bytes()
    assert data.startswith(b'\xed\xa5\x8d\xbf Chishm\0')
    assert data[0x60:0x68] == b'NRWR\x21\0\0\0'
    assert SIGNATURE not in data
    base, end = struct.unpack_from('<II', data, 0x40)
    bss_end, = struct.unpack_from('<I', data, 0x5c)
    assert end - base == len(data)
    entries = struct.unpack_from('<6I', data, 0x68)
    return Image([(base, bss_end - base, data)], dict(zip(
        ('startup', 'inserted', 'read', 'write', 'clear', 'shutdown'), entries)))


def load_recovery(path):
    from elftools.elf.elffile import ELFFile

    names = {
        'startup': 'startup', 'shutdown': 'shutdown', 'read': 'read', 'write': 'write',
        'mode': 'select_mode', 'info': 'get_info', 'workspace': 'bind_workspace',
        'error': 'get_write_error', 'repair': 'repair_table',
        'safe_ready': 'safe_mode_ready', 'safe_install': 'install_safe_mode',
    }
    with path.open('rb') as source:
        elf = ELFFile(source)
        symbols = elf.get_section_by_name('.symtab')
        regions = [(section['sh_addr'], section['sh_size'],
                    b'' if section['sh_type'] == 'SHT_NOBITS' else section.data())
                   for section in elf.iter_sections() if section['sh_flags'] & 2 and section['sh_size']]
        entries = {key: symbols.get_symbol_by_name('nrio_recovery_' + name)[0]['st_value']
                   for key, name in names.items()}
    return Image(regions, entries)


class Flash:
    def __init__(self, image, nand_id=0x1500d3ec):
        self.entries = image.entries
        self.vm = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.vm.mem_map(RAM, 0x100000)
        self.vm.mem_map(CARD, 0x1000)
        self.vm.mem_map(FIFO & ~0xfff, 0x1000)
        mapped_pages = set()
        for address, size, data in image.regions:
            for page in range(address & ~0xfff, address + size, 0x1000):
                if page not in mapped_pages:
                    self.vm.mem_map(page, 0x1000)
                    mapped_pages.add(page)
            if data:
                self.vm.mem_write(address, data)
        self.nand_id = nand_id
        self.pages_per_block = 128 if nand_id & 0x20000000 else 64
        self.flash = {}
        self.rows_seen = []
        self.erased_blocks = []
        self.programmed_rows = []
        self.failure = None
        self.status = 0xc0
        self.opcode = self.page = self.column = self.words = 0
        self.spare = self.stream_offset = 0
        self.transmitting = False
        self.program = bytearray()
        self.vm.hook_add(UC_HOOK_MEM_READ, self.read_control, begin=CTRL, end=CTRL + 3)
        self.vm.hook_add(UC_HOOK_MEM_READ, self.read_fifo, begin=FIFO, end=FIFO + 3)
        self.vm.hook_add(UC_HOOK_MEM_WRITE, self.issue, begin=CTRL, end=CTRL + 3)
        self.vm.hook_add(UC_HOOK_MEM_WRITE, self.write_fifo, begin=FIFO, end=FIFO + 3)

    def record(self, row):
        return self.flash.get(row, b'\xff' * PAGE_BYTES)

    def read_control(self, uc, access, addr, size, value, user):
        uc.mem_write(CTRL, struct.pack('<I', 0x80800000 if self.words else 0))

    def issue(self, uc, access, addr, size, value, user):
        assert not self.words
        command = bytes(uc.mem_read(COMMAND, 8))
        self.command = command[0]
        self.words = {1: 128, 7: 1}[(value >> 24) & 7]
        self.transmitting = self.command == 0xd3
        if self.transmitting:
            self.program.append(command[7])
        if self.command != 0xd1:
            return
        parameter = int.from_bytes(command[2:6], 'little')
        if command[1] == 0x61:
            self.opcode = command[2]
            if self.opcode == 0x80:
                self.program.clear()
            elif self.opcode == 0xd0:
                self.erased_blocks.append(self.page)
                self.status = 0xc1 if self.failure == 'erase' else 0xc0
                if self.status == 0xc0:
                    for row in range(self.page, self.page + self.pages_per_block):
                        self.flash.pop(row, None)
            elif self.opcode == 0x10:
                assert len(self.program) == PAGE_BYTES
                self.programmed_rows.append(self.page)
                self.status = 0xc1 if self.failure == 'program' else 0xc0
                if self.status == 0xc0:
                    if self.failure == 'readback':
                        self.program[0] ^= 1
                    self.flash[self.page] = bytes(self.program)
        elif command[1] == 0x55:
            self.page = (parameter >> 16) | (command[6] << 16)
            self.rows_seen.append(self.page)
            self.spare = bool(parameter & 0x800)
            self.stream_offset = 2048 if self.spare else 0
        elif command[1] == 0x52:
            self.stream_offset = parameter
        elif command[1] == 0x53:
            self.page = parameter
            assert not self.page % self.pages_per_block
        elif command[1] == 0x44:
            self.program.extend(command[2:6])

    def read_fifo(self, uc, access, addr, size, value, user):
        assert self.words and not self.transmitting
        self.words -= 1
        reply = 0
        if self.command == 0xc2:
            reply = 0x80808080
        elif self.command == 0xd0:
            if self.opcode == 0x90:
                reply = self.nand_id
            elif self.opcode == 0x70:
                reply = self.status
            else:
                reply, = struct.unpack_from('<I', self.record(self.page), self.stream_offset)
                self.stream_offset += 4
        uc.mem_write(FIFO, struct.pack('<I', reply))

    def write_fifo(self, uc, access, addr, size, value, user):
        assert self.words and self.transmitting
        self.words -= 1
        data = struct.pack('<I', value)
        self.program.extend(data if self.words else data[:3])

    def run(self, entry, *args):
        for reg, val in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2), args):
            self.vm.reg_write(reg, val)
        self.vm.reg_write(UC_ARM_REG_SP, STOP - 4096)
        self.vm.reg_write(UC_ARM_REG_LR, STOP)
        self.vm.emu_start(self.entries[entry], STOP, count=60_000_000)
        pc = self.vm.reg_read(UC_ARM_REG_PC)
        assert pc == STOP, f"Driver did not return: entry={entry}, pc={pc:#010x}"
        return self.vm.reg_read(UC_ARM_REG_R0)


def select_mode(device, mode):
    return device.run('mode', mode)


def started_device(image, nand_id, mode=None):
    device = Flash(image, nand_id)
    assert device.run('startup') == 1
    if mode is not None:
        assert select_mode(device, mode)
    return device


def check(image, writable, nand_id, mode=None):
    device = started_device(image, nand_id, mode)
    assert device.run('info', RAM) == 1
    info = struct.unpack('<III', device.vm.mem_read(RAM, 12))
    assert info == (2_097_152, device.pages_per_block * PAGE_BYTES if writable else 0, nand_id)
    device.flash[0] = b'\x11' * 512 + b'\x22' * 1536 + b'\xff' * 64
    device.flash[1] = b'\x33' * 2048 + b'\xff' * 64
    assert device.run('read', 0, 1, RAM + 1) == 1
    assert device.vm.mem_read(RAM + 1, 512) == b'\x11' * 512
    assert device.run('read', info[0] - 1, 1, RAM) == 1
    assert device.rows_seen[-1] == info[0] // 4 - 1
    assert not device.run('read', info[0] - 1, 2, RAM)
    assert not device.run('write', 0, 1, RAM)
    if writable:
        workspace = RAM + 0x10000
        assert not device.run('workspace', workspace, info[1] - 1)
        assert device.run('workspace', workspace, info[1])
        payload = bytes(range(256)) * 2
        device.vm.mem_write(RAM, payload)
        assert device.run('write', 0, 1, RAM) == 1
        assert device.erased_blocks == [0]
        assert device.record(0)[:512] == payload
        assert device.record(0)[512:2048] == b'\x22' * 1536
        assert device.record(1) == b'\x33' * 2048 + b'\xff' * 64
        assert device.run('write', 0, 1, RAM) == 1
        assert device.erased_blocks == [0]
        bad_row = device.pages_per_block
        device.flash[bad_row] = b'\xff' * 2048 + b'\x00' + b'\xff' * 63
        assert not device.run('write', bad_row * 4, 1, RAM)
        assert device.erased_blocks == [0]
        assert not device.run('write', 0, 1, RAM)
    else:
        assert not device.run('workspace', RAM, 128 * PAGE_BYTES)
        assert not device.erased_blocks


def recovery_device(image, nand_id, mode):
    device = started_device(image, nand_id, mode)
    assert device.run('workspace', RAM + 0x10000, device.pages_per_block * PAGE_BYTES)
    return device


def check_cross_block(image, nand_id):
    device = recovery_device(image, nand_id, MAIN_WRITE)
    page = device.pages_per_block
    preserved_spare = b'\xff' + bytes(range(1, 64))
    device.flash[page - 1] = b'\x33' * 2048 + preserved_spare
    device.flash[page] = b'\x44' * 2048 + preserved_spare
    offset = page * 4 - 1
    payload = b'\x06' * 512 + b'\xe9' * 512
    device.vm.mem_write(RAM, payload)
    assert device.run('write', offset, 2, RAM) == 1
    assert device.erased_blocks == [0, page]
    assert device.programmed_rows == [page - 1, page]
    assert device.record(page - 1)[:1536] == b'\x33' * 1536
    assert device.record(page)[512:2048] == b'\x44' * 1536
    assert device.record(page - 1)[2048:2096] == preserved_spare[:48]
    assert device.record(page)[2064:] == preserved_spare[16:]
    assert device.run('read', offset, 2, RAM) == 1
    assert device.vm.mem_read(RAM, 1024) == payload


def write_error(device):
    device.run('error', RAM + 0x2000)
    return struct.unpack('<III', device.vm.mem_read(RAM + 0x2000, 12))


def check_banner_preflight(image, nand_id):
    device = recovery_device(image, nand_id, MAIN_WRITE)
    device.flash[1] = b'\xff' * 2048 + b'\x00' + b'\xff' * 63
    device.vm.mem_write(RAM, b'\x23' * 2048)
    assert not device.run('write', 16, 4, RAM)
    assert write_error(device) == (2, 1, 0)
    assert not device.erased_blocks and not device.programmed_rows


def check_failure(image, nand_id, failure, mode, pages_written=1):
    device = recovery_device(image, nand_id, mode)
    device.failure = failure
    device.vm.mem_write(RAM, b'\x23' * 512)
    assert not device.run('write', 0, 1, RAM)
    assert device.erased_blocks == [0]
    expected_rows = {'erase': [], 'program': [0], 'readback': list(range(pages_written))}
    assert device.programmed_rows == expected_rows[failure]
    assert write_error(device) == ({'erase': 3, 'program': 4, 'readback': 6}[failure], 0, 0)
    assert not device.run('write', device.pages_per_block * 4, 1, RAM)
    assert device.erased_blocks == [0]
    assert device.run('read', 0, 1, RAM) == 1  # Dumping remains available.


def check_spare(image, nand_id, pages):
    device = started_device(image, nand_id, SPARE_READ)
    assert device.run('info', RAM) == 1
    info = struct.unpack('<III', device.vm.mem_read(RAM, 12))
    assert info == (pages // 8, 0, nand_id)
    for page in range(16):
        device.flash[page] = b'\x31' * 2048 + bytes([page]) * 64
    assert device.run('read', 0, 2, RAM + 1)
    assert device.vm.mem_read(RAM + 1, 1024) == b''.join(bytes([i]) * 64 for i in range(16))
    assert device.rows_seen == list(range(16))
    assert device.run('read', info[0] - 1, 1, RAM)
    assert device.rows_seen[-8:] == list(range(pages - 8, pages))
    assert not device.run('read', info[0] - 1, 2, RAM)
    assert not device.run('workspace', RAM, 128 * PAGE_BYTES)
    assert not device.run('write', 0, 1, RAM)
    assert not device.erased_blocks and not device.programmed_rows


def check_spare_write(image, nand_id):
    device = started_device(image, nand_id, SPARE_WRITE)
    assert device.run('info', RAM)
    assert struct.unpack('<III', device.vm.mem_read(RAM, 12)) == (
        65_536, device.pages_per_block * PAGE_BYTES, nand_id,
    )
    page = device.pages_per_block
    first_row = page - 8
    payload = b''.join(b'\x55' + bytes([i]) * 63 for i in range(16))
    for i in range(16):
        device.flash[first_row + i] = bytes([i + 1]) * 2048 + b'\xff' * 64
    neighbor = b'\xfe' * 2048 + b'\xff' * 64
    device.flash[first_row - 1] = neighbor
    device.flash[first_row + 16] = neighbor
    device.vm.mem_write(RAM, payload)
    assert not device.run('write', first_row // 8, 2, RAM)
    assert device.run('workspace', RAM + 0x10000, page * PAGE_BYTES)
    assert not device.run('write', 65_535, 2, RAM)
    assert not device.erased_blocks
    assert device.run('write', first_row // 8, 2, RAM)
    assert device.erased_blocks == [0, page]
    for i in range(16):
        assert device.record(first_row + i) == bytes([i + 1]) * 2048 + payload[i * 64:(i + 1) * 64]
    assert device.record(first_row - 1) == neighbor
    assert device.record(first_row + 16) == neighbor
    assert device.run('read', first_row // 8, 2, RAM)
    assert device.vm.mem_read(RAM, 1024) == payload
    assert device.run('write', first_row // 8, 2, RAM)
    assert device.erased_blocks == [0, page]


def check_protected(image, nand_id):
    device = recovery_device(image, nand_id, PROTECTED)
    first_writable = device.pages_per_block * 4
    boot_page = b'\x12' * PAGE_BYTES
    device.flash[0] = boot_page
    payload = b'\x23' * 2048
    device.vm.mem_write(RAM, payload)
    for lba, count in ((0, 1), (15, 2), (16, 4), (first_writable - 1, 2)):
        assert not device.run('write', lba, count, RAM)
        assert not device.erased_blocks and not device.programmed_rows
    assert device.run('read', 0, 1, RAM + 0x2000)
    assert device.vm.mem_read(RAM + 0x2000, 512) == boot_page[:512]
    assert device.run('write', first_writable, 1, RAM)
    assert device.erased_blocks == [device.pages_per_block]
    assert device.programmed_rows == [device.pages_per_block]
    assert device.record(0) == boot_page
    assert device.record(device.pages_per_block)[:512] == payload[:512]


def check_banner(template, nand_id):
    device = started_device(template, nand_id)
    assert not select_mode(device, BANNER)
    assert device.run('info', RAM)
    assert struct.unpack_from('<I', device.vm.mem_read(RAM, 12), 4)[0] == 0
    assert not device.run('workspace', RAM + 0x10000, device.pages_per_block * PAGE_BYTES)
    assert not device.run('write', 16, 4, RAM)
    assert not device.erased_blocks

    original_spare = bytes([0xbb]) + bytes(range(1, 64))
    snapshot = original_spare * 8
    new_main = b'\x31' * 2048
    new_spare = bytes(range(63, -1, -1))
    image = template.armed(snapshot, new_main, new_spare)
    device = recovery_device(image, nand_id, BANNER)
    device.vm.mem_write(RAM, new_main)
    for page in range(8):
        device.flash[page] = bytes([page]) * 2048 + original_spare
    device.flash[8] = b'\x60' * 2112
    for lba, count in ((0, 4), (15, 4), (16, 1), (16, 5), (17, 4), (256, 4)):
        assert not device.run('write', lba, count, RAM)
    device.vm.mem_write(RAM, b'\x40' * 2048)
    assert not device.run('write', 16, 4, RAM)
    assert not device.erased_blocks and not device.programmed_rows

    device.vm.mem_write(RAM, new_main)
    assert device.run('write', 16, 4, RAM)
    assert device.erased_blocks == [0]
    assert device.record(4) == new_main + new_spare
    for page in (0, 1, 2, 3, 5, 6, 7):
        assert device.record(page) == bytes([page]) * 2048 + original_spare
    assert device.record(8) == b'\x60' * 2112

    device = recovery_device(image, nand_id, BANNER)
    for page in range(8):
        device.flash[page] = b'\xff' * 2048 + original_spare
    device.flash[1] = device.record(1)[:-1] + b'\xff'
    device.vm.mem_write(RAM, new_main)
    assert not device.run('write', 16, 4, RAM)
    assert write_error(device) == (2, 1, 0xbb)
    assert not device.erased_blocks and not device.programmed_rows


def check_toolkit(image, chip):
    device = Flash(image, chip)
    device.vm.mem_write(CARD + 0x204, b'\x80\x08')
    assert device.run('startup')
    assert device.vm.mem_read(CARD + 0x204, 2) == b'\x80\x00'  # Claim Slot-1 only.
    assert not select_mode(device, SAFE_MODE + 1)
    assert device.run('read', 0, 1, RAM)
    assert not select_mode(device, MAIN_WRITE)
    assert device.run('startup')
    assert select_mode(device, SPARE_READ)
    assert not select_mode(device, MAIN_WRITE)
    assert not device.run('workspace', RAM, 128 * PAGE_BYTES)
    assert not device.run('write', 0, 1, RAM)
    assert not device.erased_blocks
    assert device.run('startup')
    assert select_mode(device, MAIN_READ)
    assert not select_mode(device, SPARE_READ)

    check(image, False, chip)
    check(image, True, chip, MAIN_WRITE)
    check_cross_block(image, chip)
    check_banner_preflight(image, chip)
    check_spare(image, chip, 524_288)
    check_spare_write(image, chip)
    check_protected(image, chip)
    check_banner(image, chip)
    for failure in ('erase', 'program', 'readback'):
        check_failure(image, chip, failure, mode=MAIN_WRITE)
        check_failure(image, chip, failure, pages_written=8, mode=SPARE_WRITE)


def check_same_cart_repair(image, chip):
    primary_page, secondary_page = 16, 128
    shared = bytearray(b'\xff' * 512)
    shared[64] = 0  # The known ARM7 record is in a usual NAND marker position.
    table = b''.join(struct.pack('<I', 0x80000 + i * 0x4000) for i in range(4096))

    def setup():
        device = recovery_device(image, chip, TABLE_REPAIR)
        for i in range(8):
            device.flash[i] = bytes([i]) * 2048 + shared[i * 64:(i + 1) * 64]
            device.flash[primary_page + i] = b'\xff' * PAGE_BYTES
            device.flash[secondary_page + i] = table[i * 2048:(i + 1) * 2048] + b'\xaa' * 64
        device.flash[24] = b'\x51' * PAGE_BYTES
        device.vm.mem_write(RAM, table + shared)
        return device

    device = setup()
    before = [device.record(page) for page in range(device.pages_per_block)]
    assert not device.run('write', 64, 32, RAM)
    assert device.run('repair', RAM)
    assert device.erased_blocks == [0]
    for i in range(8):
        assert device.record(primary_page + i)[:2048] == table[i * 2048:(i + 1) * 2048]
        assert device.record(secondary_page + i) == table[i * 2048:(i + 1) * 2048] + b'\xaa' * 64
    for page, original in enumerate(before):
        if not primary_page <= page < primary_page + 8:
            assert device.record(page) == original
    assert device.run('repair', RAM)
    assert device.erased_blocks == [0]

    device = setup()
    device.vm.mem_write(RAM, bytes([table[0] ^ 1]))
    assert not device.run('repair', RAM)
    assert write_error(device) == (7, secondary_page, 0)
    assert not device.erased_blocks

    device = setup()
    device.vm.mem_write(RAM + len(table) + 65, b'\x00')
    assert not device.run('repair', RAM)
    assert write_error(device) == (2, 1, 0)
    assert not device.erased_blocks

    device = recovery_device(image, chip, MAIN_WRITE)
    assert not device.run('repair', RAM)
    assert not device.erased_blocks


def check_safe_mode_install(image, chip, records, sectors, replacements):
    plan = (struct.pack('<III', len(records), *sectors) + b''.join(records).ljust(128 * PAGE_BYTES, b'\0')
            + b''.join(replacements))
    regions = []
    for address, size, data in image.regions:
        start = data.find(SAFE_MODE_SIGNATURE)
        if start != -1:
            start += len(SAFE_MODE_SIGNATURE)
            end = start + len(plan)
            assert data[start:end] == bytes(len(plan))
            data = data[:start] + plan + data[end:]
        regions.append((address, size, data))
    armed = Image(regions, image.entries)

    def setup(app=armed):
        device = recovery_device(app, chip, SAFE_MODE)
        device.flash.update(enumerate(records))
        device.flash[len(records)] = b'\x88' * PAGE_BYTES
        return device

    device = setup(image)
    assert not device.run('safe_ready')
    assert not device.run('safe_install') and not device.erased_blocks

    device = setup()
    assert device.run('safe_ready')
    assert not device.run('write', sectors[0], 1, RAM)
    assert device.run('safe_install')
    assert device.erased_blocks == [0]
    changed = {sector // 4: (sector % 4, replacement) for sector, replacement in zip(sectors, replacements)}
    for page, original in enumerate(records):
        stored = device.record(page)
        if page not in changed:
            assert stored == original
            continue
        column, replacement = changed[page]
        main_start = column * 512
        assert stored[:2048] == original[:main_start] + replacement + original[main_start + 512:2048]
        ecc = 2048 + column * 16
        assert stored[2048:ecc] == original[2048:ecc]
        assert stored[ecc + 16:] == original[ecc + 16:]
        for index in (0, 1, 2, 3, 7, 11, 15):
            assert stored[ecc + index] == original[ecc + index]
        assert stored[ecc + 4:ecc + 7] == stored[ecc + 8:ecc + 11] == stored[ecc + 12:ecc + 15]
    assert device.record(len(records)) == b'\x88' * PAGE_BYTES

    for offset, detail in ((17, 0), (2048 + 51, 1)):
        device = setup()
        page = len(records) - 1
        record = bytearray(records[page])
        record[offset] ^= 1
        device.flash[page] = bytes(record)
        assert not device.run('safe_install')
        assert write_error(device) == (8, page, detail)
        assert not device.erased_blocks

    old_locations = SAFE_MODE_SIGNATURE + plan[:12]
    new_locations = SAFE_MODE_SIGNATURE + struct.pack('<III', len(records), sectors[0], sectors[1] + 1)
    bad_plan = Image([(address, size, data.replace(old_locations, new_locations, 1))
                      for address, size, data in armed.regions], armed.entries)
    device = setup(bad_plan)
    assert not device.run('safe_install')
    assert write_error(device)[0] == 9 and not device.erased_blocks

    device = setup()
    device.failure = 'readback'
    assert not device.run('safe_install')
    assert write_error(device)[0] == 6 and device.erased_blocks == [0]


def check_safe_mode_fixture(image):
    sectors = (0x100, 0x1a1)
    records = [bytearray(b'\xff' * PAGE_BYTES) for _ in range(128)]
    records[1][2048] = 0
    table = records[16]
    struct.pack_into('<I', table, 0, 0x20000)
    struct.pack_into('<I', table, 5 * 4, 0x34000)
    header = records[sectors[0] // 4]
    struct.pack_into('<I', header, 0, 0xea00002e)
    header[0x0c:0x10] = b'####'
    struct.pack_into('<4I', header, 0x30, 0x14200, 0x03800000, 0x03800000, 0x38)
    struct.pack_into('<II', header, 0x80, 0x14e40, 0x200)
    replacements = (b'\x13' * 512, b'\x37' * 512)
    check_safe_mode_install(image, 0x2514d3ec, [bytes(record) for record in records], sectors, replacements)


def check_dldi(image, chip):
    device = started_device(image, chip)
    assert device.run('inserted') and device.run('clear')
    device.flash[0] = b'\x17' * 2048 + b'\xff' * 64
    assert device.run('read', 0, 1, RAM)
    assert device.vm.mem_read(RAM, 512) == b'\x17' * 512
    assert device.run('read', 2_097_151, 1, RAM)
    assert device.rows_seen[-1] == 524_287
    assert not device.run('read', 2_097_151, 2, RAM)
    for lba in range(0xfffffffb, 0x100000000):
        assert not device.run('read', lba, 0, RAM)
    assert not device.run('write', 0, 1, RAM)
    assert not device.erased_blocks and not device.programmed_rows
    assert device.run('shutdown') and not device.run('inserted')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app-elf', type=Path, help='unstripped ARM9 ELF from the USB app build')
    args = parser.parse_args()
    dldi = load_dldi(Path(__file__).resolve().parent / 'compiled/write-enabled/nrio-raw-toolkit.dldi')
    for chip in (0x1500d3ec, 0x3500d3ec):
        check_dldi(dldi, chip)
    print('Raw DLDI: physical reads, range checks, and write refusal passed')
    if args.app_elf:
        toolkit = load_recovery(args.app_elf)
        for chip in (0x1500d3ec, 0x3500d3ec):
            check_toolkit(toolkit, chip)
            check_same_cart_repair(toolkit, chip)
        check_spare(toolkit, 0x3500d5ec, 1_048_576)
        print('Recovery app: all modes, same-cart repair, and write protections passed')
        check_safe_mode_fixture(toolkit)
        print('Safe-mode installer: exact block preflight, ECC scope, and readback verification passed')
