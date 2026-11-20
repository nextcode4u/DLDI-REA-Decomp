"""Compare NRIO implementations in an ARM926 with scripted Slot-1 I/O."""

import random
import struct
import argparse
from pathlib import Path

from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_HOOK_CODE
from unicorn.arm_const import UC_CPU_ARM_926, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

BASE = 0xbf800000
RELOCATED_BASE = 0x02300000
ORIGINAL_BSS = 0x4d0c
RAM = 0x02000000
STACK = 0x02020000
STOP = 0x02030000
CARD = 0x04000000
CTRL = 0x040001a4
FIFO = 0x04100010
BSS_LENGTH = 0x5ba4 - ORIGINAL_BSS
PTR_OFFSETS = (0x4d38, 0x4d84, 0x4dec, 0x5264, 0x5aec)
REGS = (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

def get_symbols(path):
    """Read the symbol table of the unstripped ARM ELF from the build."""
    image = path.read_bytes()
    if image[:6] != b"\x7fELF\x01\x01":
        raise ValueError(f"{path} is not a little-endian ELF32 file")
    table = struct.unpack_from("<I", image, 32)[0]
    section_size, sections = struct.unpack_from("<HH", image, 46)
    headers = [struct.unpack_from("<10I", image, table + index * section_size)
               for index in range(sections)]
    symbols = {}
    for _, section_type, _, _, offset, length, link, _, _, entry_size in headers:
        if section_type != 2:
            continue
        strings = headers[link][4]
        for pos in range(offset, offset + length, entry_size):
            name, value, _, _, _, section = struct.unpack_from("<IIIBBH", image, pos)
            if name and section:
                start = strings + name
                end = image.index(0, start)
                symbols[image[start:end].decode()] = value
    return symbols


def relocate(image, new_base):
    """Apply the header and GOT relocations used by the NRIO DLDI patch."""
    image = bytearray(image)
    delta = new_base - BASE
    _, _, glue_start, glue_end, got_start, got_end, _, bss_end = struct.unpack_from("<8I", image, 0x40)
    assert image[0x0e] == 0x0e and glue_start == glue_end
    for offset in (*range(0x40, 0x60, 4), *range(0x68, 0x80, 4)):
        addr, = struct.unpack_from("<I", image, offset)
        struct.pack_into("<I", image, offset, addr + delta)
    for offset in range(got_start - BASE, got_end - BASE, 4):
        addr, = struct.unpack_from("<I", image, offset)
        if BASE <= addr < bss_end:
            struct.pack_into("<I", image, offset, addr + delta)
    return bytes(image)


class Driver:
    def __init__(self, path, seed, load_base):
        self.symbols = get_symbols(path / "nrio.elf")
        image = (path / "nrio.dldi").read_bytes()
        if load_base != BASE:
            image = relocate(image, load_base)
            self.symbols = {
                name: value + load_base - BASE if BASE <= value <= BASE + 0x8000 else value
                for name, value in self.symbols.items()
            }
        self.bss = self.symbols['__bss_start']
        assert self.symbols['__bss_end'] - self.bss == BSS_LENGTH
        self.image_end = self.bss + BSS_LENGTH
        self.vm = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.vm.ctl_set_cpu_model(UC_CPU_ARM_926)
        for start, size in ((load_base, 0x10000), (RAM, 0x40000), (CARD, 0x1000), (FIFO & ~0xfff, 0x1000)):
            self.vm.mem_map(start, size)
        self.vm.mem_write(load_base, image)
        self.seed = seed
        rand = random.Random(seed)
        self.vm.mem_write(RAM, rand.randbytes(4096))
        self.vm.mem_write(self.daddr(0x5000), rand.randbytes(64))
        self.vm.mem_write(self.daddr(0x52a8), rand.randbytes(2048))
        self.fifo_random = random.Random(0x9999 + seed)
        self.mmio = []
        self.calls = []
        self.return_script = {}
        self.test_entry = None
        self.counters = {}
        self.vm.hook_add(UC_HOOK_MEM_READ, self.read_card, begin=CARD, end=FIFO + 3)
        self.vm.hook_add(UC_HOOK_MEM_WRITE, self.write_card, begin=CARD, end=FIFO + 3)
        self.vm.hook_add(UC_HOOK_CODE, self.watch_subroutine)
        self.stubbed = {}
        self.set32(0x4d38, RAM + 0x800)
        self.set32(0x5264, RAM + 0x400)
        self.set32(0x4d84, self.daddr(0x5050))
        self.set32(0x4dec, self.daddr(0x5b44))
        self.set32(0x5aec, self.daddr(0x5afc))
        self.set32(0x4d28, 0x80000)
        self.set32(0x4df0, 0x800000)
        self.set8(0x4dfc, 4)
        self.set8(0x5af0, 3)
        self.set8(0x5b88, 2)
        self.set8(0x5b8c, 0xff)
        self.set8(0x5b94, 0xff)
        self.set8(0x4da0, 0xff)
        self.set32(0x5b84, 1)
        self.vm.mem_write(self.daddr(0x5afc), b'\xff' * 64)
        self.vm.mem_write(self.daddr(0x5b44), b'\xff' * 64)

    def daddr(self, offset):
        return self.bss + offset - ORIGINAL_BSS

    def set8(self, offset, value):
        self.vm.mem_write(self.daddr(offset), bytes((value,)))

    def set16(self, offset, value):
        self.vm.mem_write(self.daddr(offset), struct.pack('<H', value))

    def set32(self, offset, value):
        self.vm.mem_write(self.daddr(offset), struct.pack('<I', value))

    def get8(self, offset):
        return self.vm.mem_read(self.daddr(offset), 1)[0]

    def get32(self, offset):
        return struct.unpack('<I', self.vm.mem_read(self.daddr(offset), 4))[0]

    def norm(self, value):
        if self.bss <= value < self.image_end:
            return 'bss', value - self.bss
        if STACK - 512 <= value < STACK + 32:
            return 'stack'
        for symbol in ('mftl_sectors', 'mftl_spare', 'mftl_blocktab', 'mftl_freetab', 'mftl_freeptr'):
            if self.symbols.get(symbol) == value:
                return symbol
        return value

    def out(self, result):
        state = bytearray(self.vm.mem_read(self.bss, BSS_LENGTH))
        ptr = {offset: self.norm(self.get32(offset)) for offset in PTR_OFFSETS}
        for offset in PTR_OFFSETS:
            index = offset - ORIGINAL_BSS
            state[index:index + 4] = bytes(4)
        return (result, bytes(state), ptr, bytes(self.vm.mem_read(RAM, 4096)), self.calls, self.mmio)

    def read_card(self, uc, access, addr, size, val, user):
        if addr == CTRL:
            uc.mem_write(CTRL, struct.pack('<I', 0x00800000))
        if addr == FIFO:
            data = self.fifo_random.getrandbits(32)
            uc.mem_write(FIFO, struct.pack('<I', data))
            self.mmio.append(('fifo-read', data))

    def write_card(self, uc, access, addr, size, val, user):
        self.mmio.append(('write', addr, size, val))

    def lookup_status(self, name, default):
        script = self.return_script.get(name, [])
        off = self.counters.get(name, 0)
        self.counters[name] = off + 1
        return script[off] if off < len(script) else default

    def stub(self, **calls):
        for label, argc in calls.items():
            self.stubbed[self.symbols[label]] = (label, argc)

    def watch_subroutine(self, uc, addr, size, user):
        if addr not in self.stubbed:
            return
        name, argc = self.stubbed[addr]
        vals = [uc.reg_read(reg) for reg in REGS[:argc]]
        self.calls.append((name, tuple(self.norm(v) for v in vals)))
        default = 0
        if name == 'nrio_allocate_free_block':
            default = 5
        if name == 'nrio_find_free_block':
            default = 13
        if name == 'nrio_find_block_table':
            default = 1
        outcome = self.lookup_status(name, default)
        if name == 'nrio_read_nand_id':
            device_ids = (0x71, 0x74, 0xc1, 0xc3, 0xc5, 0xca, 0xcc, 0xd3, 0xd5, 0xdc)
            uc.mem_write(vals[0], bytes((0xec, device_ids[self.seed % len(device_ids)],
                                        0, (self.seed // 10) << 5)))
        if name == 'nrio_page_operation':
            if vals[0] in (1, 10):
                self.set8(0x500b, 0xab if self.seed & 1 else 0xff)
                if self.test_entry == 'nrio_startup':
                    for offset, byte in ((0, 0x55), (1, 0xaa), (7, 0xab), (11, 0xab), (15, 0xab)):
                        self.set8(0x5000 + offset, byte)
            if vals[0] == 11 and self.test_entry == 'nrio_read_sectors':
                sector = self.get32(0x5044)
                uc.mem_write(self.get32(0x5264), bytes((sector & 0xff,)) * 512)
        if name == 'nrio_classify_physical_block':
            if vals[0] == 3 and (self.seed & 1):
                self.set8(0x5b8c, 3)
                self.set8(0x5b94, 2)
                self.vm.mem_write(self.get32(0x5aec) + 2, bytes((4,)))
            outcome = vals[0] ^ 0xff
        if name == 'nrio_load_physical_block_table':
            pb = self.get32(0x4d84)
            for index in range(64):
                self.vm.mem_write(pb + index * 4, struct.pack('<I', index if index < 63 else 4096))
        if name == 'nrio_clear_block_map':
            self.vm.mem_write(self.get32(0x5aec), b'\xff' * 62)
        if name == 'nrio_clear_free_table':
            self.vm.mem_write(self.get32(0x4dec), b'\xff' * 64)
        if name == 'nrio_read_nand_sector_with_ecc':
            uc.mem_write(vals[0], bytes([vals[1]]) * 512)
            uc.mem_write(vals[2], b'\x10\x20\x30')
        if name == 'nrio_correct_ecc':
            self.calls[-1] += (bytes(uc.mem_read(vals[1], 3)),)
        uc.reg_write(UC_ARM_REG_R0, outcome)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def run(self, label, args, use_r0):
        for reg, value in zip(REGS, args):
            self.vm.reg_write(reg, value)
        self.vm.reg_write(UC_ARM_REG_SP, STACK)
        self.vm.reg_write(UC_ARM_REG_LR, STOP)
        self.vm.emu_start(self.symbols[label], STOP, count=250000)
        if self.vm.reg_read(UC_ARM_REG_PC) != STOP:
            raise RuntimeError(f'Unterminated {label} seed={self.seed}')
        return self.out(self.vm.reg_read(UC_ARM_REG_R0) if use_r0 else None)

def case(driver, label, n):
    driver.test_entry = label
    if label == 'nrio_startup':
        driver.stub(nrio_read_nand_id=1, nrio_page_operation=1,
                    nrio_load_physical_block_table=0, nrio_classify_physical_block=1)
        return (), True
    if label == 'nrio_read_sectors':
        driver.set32(0x4d7c, 2048)
        driver.stub(nrio_load_zone=0, nrio_select_physical_page=2, nrio_page_operation=1)
        return (n, 3, RAM + 0x400), True
    if label == 'nrio_finish_nand_write':
        if n & 4:
            driver.set32(0x4d28, 0)
        driver.stub(nrio_send_nand_command=1, nrio_wait_nand=0)
        return (RAM,), False
    if label == 'nrio_read_nand_sector_with_ecc':
        driver.stub(nrio_send_nand_command=1, nrio_command_52=1)
        return (RAM + (n % 4), n % 4, RAM + 0x300), False
    if label == 'nrio_write_nand_sector_with_ecc':
        driver.stub(nrio_send_nand_command=1, nrio_command_55=1)
        if n & 4:
            driver.set32(0x4d28, 0)
        return (RAM + (n % 4), n % 4, RAM + 0x300), False
    if label == 'nrio_page_operation':
        driver.set8(0x5b40, n % 4)
        driver.stub(nrio_read_nand_spare=1, nrio_read_nand_sector=2,
                    nrio_read_nand_sector_with_ecc=3, nrio_write_nand_sector_with_ecc=3,
                    nrio_finish_nand_write=1, nrio_erase_nand_block=0, nrio_correct_ecc=3,
                    nrio_write_nand_sector_raw=2)
        return (n // 4,), False
    if label == 'nrio_find_block_table':
        driver.stub(nrio_set_nand_address=1, nrio_page_operation=1, nrio_check_spare_markers=0)
        driver.set8(0x4dfc, 64)
        driver.return_script['nrio_check_spare_markers'] = [1] * (n % 10)
        return (), True
    if label == 'nrio_copy_nand_pages':
        driver.stub(nrio_select_physical_page=2, nrio_page_operation=1, nrio_check_spare_markers=0,
                    nrio_set_spare_markers=0, nrio_decode_nand_status=0)
        driver.return_script['nrio_check_spare_markers'] = [255, 0, 0] if n & 1 else []
        driver.return_script['nrio_decode_nand_status'] = [0, 3] if n & 2 else []
        return (0xff if n & 4 else 7, 15, n % 4, 3), True
    if label == 'nrio_load_zone':
        driver.set8(0x4df8, n & 1)
        driver.set8(0x5040, (n >> 1) & 1)
        driver.set8(0x5b98, (n >> 2) & 1)
        driver.set16(0x4d9c, (n >> 3) & 3)
        driver.set16(0x4d90, 1)
        driver.set16(0x5af4, 2)
        driver.stub(nrio_select_physical_page=2, nrio_page_operation=1, nrio_check_spare_markers=0,
                    nrio_clear_block_map=0, nrio_load_physical_block_table=0, nrio_classify_physical_block=1)
        return (), False
    if label == 'nrio_init_nand_geometry':
        driver.stub(nrio_require_word_alignment=2, nrio_find_block_table=0,
                    nrio_load_physical_block_table=0, nrio_clear_block_map=0,
                    nrio_clear_free_table=0, nrio_load_zone=0)
        return (0x40000 if n & 1 else 0x20000, 0x7c0 + n * 64, 0x800000, 0), True
    if label in ('nrio_read_sector_with_ecc_mapping', 'nrio_write_sector_with_mapping',
                 'nrio_write_sectors'):
        driver.stub(nrio_load_zone=0, nrio_select_physical_page=2, nrio_page_operation=1,
                    nrio_check_spare_markers=0, nrio_find_free_block=0, nrio_copy_nand_pages=4,
                    nrio_replace_pending_block=0, nrio_flush_block=0, nrio_allocate_free_block=0,
                    nrio_write_page_marker=1, nrio_set_spare_markers=0, nrio_decode_nand_status=0,
                    nrio_release_block=1, nrio_commit_pending_block=0)
        driver.set32(0x5044, n % 12)
        driver.set32(0x5b84, 1 + n % 7)
        driver.set8(0x4d80, n & 1)
        driver.set8(0x5048, (n >> 1) % 4)
        driver.set8(0x5ae8, (n >> 3) % 4)
        driver.set8(0x525c, (n >> 5) % 4)
        if n & 4:
            driver.set8(0x5b94, 0)
            driver.set8(0x5b8c, 5)
        if n & 8:
            driver.set8(0x4da0, 7)
            driver.set8(0x5afc, 7)
        if n & 16:
            driver.return_script['nrio_check_spare_markers'] = [0xff]
        if n & 32:
            driver.return_script['nrio_decode_nand_status'] = [3]
        if label == 'nrio_write_sectors':
            driver.set32(0x4d7c, 2048)
            return (n % 12, 1 + n % 4, RAM + 0x800), True
        return (), True
    raise AssertionError(label)

LABELS = {
    'nrio_startup': 20,
    'nrio_read_sectors': 16,
    'nrio_write_sectors': 32,
    'nrio_finish_nand_write': 8,
    'nrio_read_nand_sector_with_ecc': 32,
    'nrio_write_nand_sector_with_ecc': 32,
    'nrio_page_operation': 52,
    'nrio_find_block_table': 24,
    'nrio_copy_nand_pages': 16,
    'nrio_load_zone': 64,
    'nrio_init_nand_geometry': 8,
    'nrio_read_sector_with_ecc_mapping': 48,
    'nrio_write_sector_with_mapping': 64,
}
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    project = Path(__file__).resolve().parent.parent
    parser.add_argument("--matching-dir", type=Path, default=project / "build/matching")
    parser.add_argument("--c-dir", type=Path, default=project / "build")
    args = parser.parse_args()
    images = (args.matching_dir, args.c_dir)
    total_failures = 0
    for load_base in (BASE, RELOCATED_BASE):
        print(f"Load address {load_base:#010x}", flush=True)
        total_failures += compare(images, load_base)
    return int(total_failures != 0)


def compare(images, load_base):
    total_failures = 0
    for label, trials in LABELS.items():
        failures = 0
        for n in range(trials):
            results = []
            for binary in images:
                try:
                    machine = Driver(binary, n, load_base)
                    args, use_result = case(machine, label, n)
                    results.append(machine.run(label, args, use_result))
                except (UcError, RuntimeError) as exc:
                    results.append(str(exc))
            if results[0] == results[1]:
                continue
            failures += 1
            if failures > 3:
                continue
            print(f'{label} scenario {n}')
            a, b = results
            if isinstance(a, str) or isinstance(b, str):
                print(a if isinstance(a, str) else 'original exited', b if isinstance(b, str) else 'C exited')
                continue
            for i,(x,y) in enumerate(zip(a,b)):
                if x == y:
                    continue
                if isinstance(x, bytes):
                    base = ORIGINAL_BSS if i == 1 else 0
                    diff = [(hex(j+base),hex(m),hex(o)) for j,(m,o) in enumerate(zip(x,y)) if m!=o]
                    print('  BSS' if i == 1 else '  RAM', len(diff), diff[:12])
                elif isinstance(x, list):
                    for j in range(max(len(x),len(y))):
                        va=x[j] if j<len(x) else '-'
                        vb=y[j] if j<len(y) else '-'
                        if va!=vb:
                            print('  calls' if i==4 else '  MMIO',len(x),len(y),'at',j,va,vb)
                            break
                else:
                    print('  result' if i==0 else '  state pointers', x, y)
        print(f'{label}: {trials-failures}/{trials} scenarios agree', flush=True)
        total_failures += failures
    return total_failures


if __name__ == "__main__":
    raise SystemExit(main())
