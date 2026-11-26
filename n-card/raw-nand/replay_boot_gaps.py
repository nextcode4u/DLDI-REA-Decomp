#!/usr/bin/env python3
"""Replay selected original ARM boot routines against external ROMs and captures.

Requires Unicorn. No code or data is sent to a console. Cartridge I/O is
modeled at function boundaries and cannot establish actual ASIC behavior.
"""

import argparse
from pathlib import Path
from struct import pack, unpack_from
import zlib

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (
    UC_ARM_REG_PC, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
)

BASE, STOP, BUFFER = 0x02000000, 0x022F0000, 0x02200000
MAILBOX, ARM7_JUMP = 0x027FFE60, 0x027FFE34
CHUNK, ALLOCATION, SECONDARY = 0x4000, 0x20000, 0x40000
SAFE_REVISIONS = {
    0xDCA3E6FC: (0x02000440, 0x02000798, 0x0200051C, 0x0200052C),
    0xFB3145A0: (0x02000448, 0x02000DBC, 0x02000524, 0x02000534),
    0x9207EA03: (0x02000448, 0x02000DBC, 0x02000524, 0x02000534),
}


def word(data, offset=0):
    return unpack_from("<I", data, offset)[0]


def section(rom, field):
    offset, entry, address, size = unpack_from("<4I", rom, field)
    if not offset or not size or offset + size > len(rom):
        raise ValueError("ROM does not contain the declared ARM section.")
    return rom[offset:offset + size], entry, address


def machine(code, address=BASE):
    vm = Uc(UC_ARCH_ARM, UC_MODE_ARM)
    for begin, size in ((BASE, 0x300000), (0x027FF000, 0x1000),
                        (0x037F8000, 0x10000), (0x04000000, 0x2000)):
        vm.mem_map(begin, size)
    vm.mem_write(address, code)
    vm.reg_write(UC_ARM_REG_SP, STOP - 0x1000)
    vm.reg_write(UC_ARM_REG_LR, STOP)
    return vm


def intercept(vm, address, callback):
    vm.hook_add(UC_HOOK_CODE, lambda uc, pc, size, data: callback(uc), begin=address, end=address)


def ret(vm):
    vm.reg_write(UC_ARM_REG_PC, vm.reg_read(UC_ARM_REG_LR))


def run_sender(rom):
    code, _, address = section(rom, 0x20)
    if address != BASE or code[0x5d8:0x5dc] != pack("<I", 0xe5832000) or word(code, 0x5e8) != MAILBOX:
        raise ValueError("This first-stage ARM9 sender is not recognized.")
    vm = machine(code)
    writes = []
    vm.hook_add(UC_HOOK_MEM_WRITE, lambda uc, access, addr, size, value, _: writes.append(value),
                begin=MAILBOX, end=MAILBOX + 3)
    vm.emu_start(BASE + 0x5c8, STOP, count=64)
    assert vm.reg_read(UC_ARM_REG_PC) == STOP and len(writes) == 1
    return writes[0]


def run_waiter(rom, value):
    code, entry, address = section(rom, 0x30)
    if len(code) != 0x38 or address != 0x03800000 or entry != address:
        raise ValueError("Expected an unmodified safe-mode ARM7 waiter.")
    vm = machine(code, address)
    vm.mem_write(MAILBOX, pack("<I", value))
    vm.mem_write(ARM7_JUMP, pack("<I", STOP))
    vm.emu_start(entry, STOP, count=2048)
    return vm.reg_read(UC_ARM_REG_PC) == STOP


def run_safe_selector(rom, capture, skip_first=False):
    code, _, address = section(rom, 0x20)
    sites = SAFE_REVISIONS.get(zlib.crc32(code))
    if address != BASE or sites is None:
        raise ValueError("Selector addresses are not verified for this ARM9 revision.")
    start, command, normal, normal_stop = sites
    qafe = int.from_bytes(b"QAFE", "little")
    scans, frames = [], []

    def read_marker(vm):
        at = vm.reg_read(UC_ARM_REG_R0)
        scans.append(at)
        value = qafe if skip_first and at == 0x3fffc else word(capture, at)
        vm.reg_write(UC_ARM_REG_R0, value)
        ret(vm)

    def command_frame(vm):
        frames.append((vm.reg_read(UC_ARM_REG_R0), vm.reg_read(UC_ARM_REG_R1),
                       vm.reg_read(UC_ARM_REG_R2)))
        ret(vm)

    vm = machine(code)
    intercept(vm, BASE + 0x2f4, read_marker)
    intercept(vm, command, command_frame)
    # Stop immediately after the original routine finishes selecting a table.
    scan_stop = start + 0x30
    vm.emu_start(start, scan_stop, count=260)
    at = scans[-1] + 4
    assert vm.reg_read(UC_ARM_REG_PC) == scan_stop and len(frames) == 3
    assert frames == [(0xc1, 0x07000000 | (at >> 24) << 16, 0),
                      (0xc1, 0x08000000 | ((at >> 16) & 255) << 16, 0),
                      (0xc1, 0x09000000 | ((at >> 8) & 0xf0) << 16, 0)]
    assert at == (0x60000 if skip_first else SECONDARY)
    vm = machine(code)
    frames.clear()
    intercept(vm, command, command_frame)
    vm.emu_start(normal, normal_stop, count=64)
    assert frames == [(0xc1, 0x10bf0000, 0)]
    return scans, at


def mapped_payload(capture):
    table = capture[SECONDARY:SECONDARY + ALLOCATION]
    entries = unpack_from("<4096I", table)
    first = entries[0]
    length = word(capture, first + 0x80)
    if length != 0x92240:
        raise ValueError("Expected the captured compact uDisk 1.45 image.")
    used = (length + CHUNK - 1) // CHUNK
    spans = [(at, min(CHUNK, length - n * CHUNK)) for n, at in enumerate(entries[:used])]
    assert all(at % CHUNK == 0 and at + size <= len(capture) for at, size in spans)
    image = b"".join(capture[at:at + size] for at, size in spans)
    active = next((i for i, at in enumerate(entries) if at == 0xffffffff), len(entries))
    assert len(image) == length and active == 512 and used == 37
    trailing = [at for at in entries[48:active] if at + CHUNK <= len(capture)]
    nonblank = sum(capture[at:at + CHUNK] != b"\xff" * CHUNK for at in trailing)
    print(f"Fire Link boot payload: {used} of {active} map entries needed by declared ROM.")
    print(f"After the six-allocation write window: {len(trailing)} complete extra chunks captured, "
          f"{nonblank} not all FF. Remaining extra chunks are not captured.")
    return table


def compare_early_fire_data(capture, early):
    """Check representative exact matches without guessing uncaptured blocks."""
    spans = ((0x188cc3, 0x6209b, 6061), (0x18a87b, 0x63c53, 4621),
             (0x18be93, 0x6526b, 11793))
    for at, old_at, length in spans:
        visible = capture[at:at + length]
        assert len(visible) == length and len(set(visible)) > 16
        assert visible == early[old_at:old_at + length]
    print(f"{sum(n for _, _, n in spans):,} extra mapped bytes match the archived early uDisk "
          "at an unaligned offset. These matches do not prove that the data is still live.")


def replay_updater(rom, payload, original_table):
    code, _, address = section(rom, 0x20)
    if address != BASE or zlib.crc32(code) != 0x104eb1b9:
        raise ValueError(f"Expected archived stock 1.45 updater ARM9; got {zlib.crc32(code):08X}.")
    if len(payload) != 6 * ALLOCATION:
        raise ValueError("Expected a six-allocation raw stock uDisk payload.")
    for geometry in (0x20000, 0x40000):
        vm = machine(code)
        vm.mem_write(0x0206258C + 12, pack("<I", geometry))
        vm.mem_write(BUFFER, payload)
        header = BUFFER + 0xff000
        vm.mem_write(header + 0x88, pack("<I", len(payload) // 512 * 515))
        vm.reg_write(UC_ARM_REG_R0, header)
        vm.reg_write(UC_ARM_REG_R1, BUFFER)
        writes = {}

        def program(uc):
            writes[uc.reg_read(UC_ARM_REG_R0)] = bytes(uc.mem_read(uc.reg_read(UC_ARM_REG_R1), ALLOCATION))
            ret(uc)

        def checksum(uc):
            uc.reg_write(UC_ARM_REG_R0, sum(writes[uc.reg_read(UC_ARM_REG_R0)]))
            ret(uc)

        intercept(vm, BASE + 0x614, program)
        intercept(vm, BASE + 0x6a4, checksum)
        for entry in (0x0200EE14, 0x0200F0D4, 0x0200C768):
            intercept(vm, entry, ret)
        vm.emu_start(BASE + 0x6dc, STOP, count=9_000_000)
        expected = [0x80000 + n * ALLOCATION for n in range(6)]
        assert vm.reg_read(UC_ARM_REG_PC) == STOP and list(writes) == expected + [SECONDARY]
        after = writes[SECONDARY]
        assert after[:48 * 4] == original_table[:48 * 4] and after[-8:] == b"\3\0\0\0gao\0"
        changed = sum(a != b for a, b in zip(unpack_from("<512I", original_table)[48:],
                                             unpack_from("<512I", after)[48:]))
        assert changed == 464
        print(f"{geometry // 1024} KiB erase: stock updater replaces all {changed} extra entries, "
              "without programming their payload addresses.")


def verify_early_sender(source, physical, original_exe, earlier_safe, newer_safe):
    # This is a fitted and then checked hypothesis, not an input to the
    # blind reconstruction run. The ciphertext is compared, never used as
    # code or as a source for the candidate bytes.
    from replay_dsflash2_prepare import prepare

    candidate = bytearray(source)
    for offset, expected in ((0x4588, 0xb918027e), (0x45e4, 0x1918027e)):
        if word(candidate, offset) != expected:
            raise ValueError("The later source does not match the known literal locations.")
        candidate[offset + 3] ^= 8
    secure = prepare(original_exe, bytes(candidate))
    assert secure[0x4000:0x4800] == physical[0x800:0x1000]
    assert secure[0x2000:0x3000] == physical[0x1000:0x2000]
    sender = run_sender(bytes(candidate))
    assert sender == 0x1118027e and run_waiter(earlier_safe, sender) and not run_waiter(newer_safe, sender)
    print("Earlier hypothesis reproduces all 2048 held-out encrypted code bytes.")
    print(f"  Its sender writes {sender:08X}. This releases the earlier waiter and stalls the newer one.")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for option in ("stage1", "earlier-safe", "newer-safe", "fire8-main", "updater", "payload"):
        p.add_argument("--" + option, type=Path, required=True)
    for option in ("legacy-logical", "legacy-main", "dsflash2"):
        p.add_argument("--" + option, type=Path)
    p.add_argument("--early-fire-payload", type=Path)
    args = p.parse_args()
    hypothesis = (args.legacy_logical, args.legacy_main, args.dsflash2)
    if any(hypothesis) and not all(hypothesis):
        p.error("the earlier hypothesis needs --legacy-logical, --legacy-main and --dsflash2")
    sender = run_sender(args.stage1.read_bytes())
    print(f"First-stage ARM9 wrote {sender:08X} to {MAILBOX:08X}.")
    capture = args.fire8_main.read_bytes()
    fire_safe = capture[word(capture, 0x8000):]
    for path, data in (("earlier DS Linker", args.earlier_safe.read_bytes()),
                       ("newer DS Linker", args.newer_safe.read_bytes()), ("Fire Link capture", fire_safe)):
        stub, _, _ = section(data, 0x30)
        expected = word(stub, 0x34)
        assert run_waiter(data, expected) and not run_waiter(data, expected ^ 0x08000000)
        print(f"{path}: ARM7 expects {expected:08X}. Recorded sender value "
              f"{'releases it' if expected == sender else 'leaves it waiting'}.")
        _, at = run_safe_selector(data, capture)
        _, next_at = run_safe_selector(data, capture, skip_first=True)
        print(f"  ARM9 scan selects {at:05X}; QAFE moves to {next_at:05X}. Normal selector command matches.")
    if all(hypothesis):
        verify_early_sender(*(path.read_bytes() for path in hypothesis),
                            args.earlier_safe.read_bytes(), args.newer_safe.read_bytes())
    table = mapped_payload(capture)
    if args.early_fire_payload:
        compare_early_fire_data(capture, args.early_fire_payload.read_bytes())
    replay_updater(args.updater.read_bytes(), args.payload.read_bytes(), table)


if __name__ == "__main__":
    main()
