"""Run the USB images' embedded ARM DLDI against a simulated Slot-1 bridge."""

import struct
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_MODE_ARM
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP,
)

CARD = 0x04000000
CTRL = CARD + 0x1a4
COMMAND = CARD + 0x1a8
FIFO = 0x04100010
RAM = 0x02100000
STOP = RAM + 0xf0000
PAGE_BYTES = 2112


class Flash:
    def __init__(self, image, nand_id=0x1500d3ec):
        header = image.index(b'\xee\xa5\x8d\xbf Chishm\0')
        base, data_end = struct.unpack_from('<II', image, header + 0x40)
        self.entries = struct.unpack_from('<6I', image, header + 0x68)
        self.vm = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.vm.mem_map(base & ~0xfff, 0x4000)
        self.vm.mem_map(RAM, 0x100000)
        self.vm.mem_map(CARD, 0x1000)
        self.vm.mem_map(FIFO & ~0xfff, 0x1000)
        self.vm.mem_write(base, image[header:header + data_end - base])
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


def check(image, writable, nand_id):
    device = Flash(image, nand_id)
    assert device.run(0) == 1
    assert device.run(2, 0xffffffff, 0, RAM) == 1
    info = struct.unpack('<IIII', device.vm.mem_read(RAM, 16))
    assert info == (1, 2_097_152, device.pages_per_block * PAGE_BYTES if writable else 0, nand_id)
    device.flash[0] = b'\x11' * 512 + b'\x22' * 1536 + b'\xff' * 64
    device.flash[1] = b'\x33' * 2048 + b'\xff' * 64
    assert device.run(2, 0, 1, RAM + 1) == 1
    assert device.vm.mem_read(RAM + 1, 512) == b'\x11' * 512
    assert device.run(2, info[1] - 1, 1, RAM) == 1
    assert device.rows_seen[-1] == info[1] // 4 - 1
    assert not device.run(2, info[1] - 1, 2, RAM)
    assert not device.run(3, 0, 1, RAM)
    if writable:
        workspace = RAM + 0x10000
        assert not device.run(2, 0xfffffffe, info[2] - 1, workspace)
        assert device.run(2, 0xfffffffe, info[2], workspace)
        payload = bytes(range(256)) * 2
        device.vm.mem_write(RAM, payload)
        assert device.run(3, 0, 1, RAM) == 1
        assert device.erased_blocks == [0]
        assert device.record(0)[:512] == payload
        assert device.record(0)[512:2048] == b'\x22' * 1536
        assert device.record(1) == b'\x33' * 2048 + b'\xff' * 64
        assert device.run(3, 0, 1, RAM) == 1
        assert device.erased_blocks == [0]
        bad_row = device.pages_per_block
        device.flash[bad_row] = b'\xff' * 2048 + b'\x00' + b'\xff' * 63
        assert not device.run(3, bad_row * 4, 1, RAM)
        assert device.erased_blocks == [0]
        assert not device.run(3, 0, 1, RAM)
    else:
        assert not device.run(2, 0xfffffffe, 128 * PAGE_BYTES, RAM)
        assert not device.erased_blocks


def recovery_device(image, nand_id):
    device = Flash(image, nand_id)
    assert device.run(0) == 1
    assert device.run(2, 0xfffffffe, device.pages_per_block * PAGE_BYTES, RAM + 0x10000)
    return device


def check_cross_block(image, nand_id):
    device = recovery_device(image, nand_id)
    page = device.pages_per_block
    preserved_spare = b'\xff' + bytes(range(1, 64))
    device.flash[page - 1] = b'\x33' * 2048 + preserved_spare
    device.flash[page] = b'\x44' * 2048 + preserved_spare
    offset = page * 4 - 1
    payload = b'\x06' * 512 + b'\xe9' * 512
    device.vm.mem_write(RAM, payload)
    assert device.run(3, offset, 2, RAM) == 1
    assert device.erased_blocks == [0, page]
    assert device.programmed_rows == [page - 1, page]
    assert device.record(page - 1)[:1536] == b'\x33' * 1536
    assert device.record(page)[512:2048] == b'\x44' * 1536
    assert device.record(page - 1)[2048:2096] == preserved_spare[:48]
    assert device.record(page)[2064:] == preserved_spare[16:]
    assert device.run(2, offset, 2, RAM) == 1
    assert device.vm.mem_read(RAM, 1024) == payload


def check_failure(image, nand_id, failure):
    device = recovery_device(image, nand_id)
    device.failure = failure
    device.vm.mem_write(RAM, b'\x23' * 512)
    assert not device.run(3, 0, 1, RAM)
    assert device.erased_blocks == [0]
    assert device.programmed_rows == ([] if failure == 'erase' else [0])
    assert not device.run(4)  # clearStatus reports the latched error.
    assert not device.run(3, device.pages_per_block * 4, 1, RAM)
    assert device.erased_blocks == [0]
    assert device.run(2, 0, 1, RAM) == 1  # Dumping remains available.


if __name__ == '__main__':
    path = Path(__file__).resolve().parent / 'compiled'
    for mode in ('readonly', 'recovery'):
        image = (path / f'nrio-raw-{mode}.nds').read_bytes()
        for chip in (0x1500d3ec, 0x3500d3ec):
            check(image, mode == 'recovery', chip)
            if mode == 'recovery':
                check_cross_block(image, chip)
                for failure in ('erase', 'program', 'readback'):
                    check_failure(image, chip, failure)
        print(f'{mode}: physical sector 0, full capacity and write policy passed on both geometries')
    print('Cross-block writes and erase, program and readback failure handling passed on both geometries')
