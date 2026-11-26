"""Run the verified DSFlash2 0.3Z image-preparation function in x86 emulation."""

from hashlib import sha256
from struct import pack, unpack_from

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

DSFLASH2_SHA256 = "24ee34db7a3944d3eaf3880c90732098dd28c02c7d7505d596d3404eaab7d38a"


def prepare(exe, rom):
    if sha256(exe).hexdigest() != DSFLASH2_SHA256:
        raise ValueError("Expected the verified DSFlash2 0.3Z executable.")
    if not 0x4800 <= len(rom) <= 0x1000000:
        raise ValueError("Unexpected logical ROM size for this isolated replay.")
    pe, = unpack_from("<I", exe, 0x3c)
    count, = unpack_from("<H", exe, pe + 6)
    opt, = unpack_from("<H", exe, pe + 20)
    base, = unpack_from("<I", exe, pe + 52)
    size, = unpack_from("<I", exe, pe + 80)
    vm = Uc(UC_ARCH_X86, UC_MODE_32)
    vm.mem_map(base, (size + 0xfff) & ~0xfff)
    for i in range(count):
        _, va, raw, at = unpack_from("<4I", exe, pe + 24 + opt + i * 40 + 8)
        if raw:
            vm.mem_write(base + va, exe[at:at + raw])

    address, stack, stop = 0x01000000, 0x02200000, 0x03000000
    vm.mem_map(address, (len(rom) + 0xfff) & ~0xfff)
    vm.mem_map(stack - 0x100000, 0x100000)
    vm.mem_map(stop, 0x1000)
    vm.mem_write(address, rom)
    vm.reg_write(UC_X86_REG_ESP, stack - 0x100)
    vm.mem_write(stack - 0x100, pack("<3I", stop, address, len(rom)))

    def status(uc, pc, instruction_size, data):
        uc.reg_write(UC_X86_REG_EIP, 0x406a80)  # Status text only.

    def scratch(uc, pc, instruction_size, data):
        sp = uc.reg_read(UC_X86_REG_ESP)
        target, = unpack_from("<I", uc.mem_read(sp, 4))
        uc.reg_write(UC_X86_REG_EAX, stop + 0x200)
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        uc.reg_write(UC_X86_REG_EIP, target)

    vm.hook_add(UC_HOOK_CODE, status, begin=0x406a7b, end=0x406a7b)
    vm.hook_add(UC_HOOK_CODE, scratch, begin=0x4c5c8c, end=0x4c5c8c)
    vm.emu_start(0x406880, stop, count=20_000_000)
    if vm.reg_read(UC_X86_REG_EIP) != stop or vm.reg_read(UC_X86_REG_EAX) != 1:
        raise ValueError("Original image preparation did not complete.")
    return bytes(vm.mem_read(address, len(rom)))
