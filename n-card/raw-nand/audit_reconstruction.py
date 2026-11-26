#!/usr/bin/env python3
"""Score independent block-0 reconstruction claims against held-out dumps.

The prediction uses only the given ROM inputs and fixed source mappings.
Held-out bytes are opened after the prediction is built. No flash image is saved.
"""

import argparse
from pathlib import Path
import struct

from patch_safe_mode import crc16, is_compact_safe_mode

PAGE, SPARE, CHUNK = 0x800, 64, 0x4000
HANDOFFS = (0x1118027E, 0x1918027E)


def make_claims(pages, prepared, logical, safe, first, handoff):
    claims = {"main": [], "spare": []}
    if prepared is not None:
        if len(prepared) < 0x4800 or prepared[0x0C:0x10] != b"DSGB":
            raise ValueError("Expected an independently prepared DSGB first-stage image.")
        claims["main"].extend([
            ("first-stage secure data", 0x800, prepared[0x4000:0x4800]),
            ("first-stage table", 0x1000, prepared[0x2000:0x3000]),
        ])
        claims["spare"].extend([
            ("split table page 2", 2 * SPARE, prepared[0x1008:0x1048]),
            ("split table page 3", 3 * SPARE, prepared[0x1000:0x1040]),
        ])
    if logical is not None:
        if len(logical) < 0x200 or handoff not in HANDOFFS:
            raise ValueError("A logical first stage needs an independently chosen --handoff.")
        arm7, _, _, length = struct.unpack_from("<4I", logical, 0x30)
        stub = bytearray(logical[arm7:arm7 + length])
        if (length != SPARE or len(stub) != SPARE or crc16(stub[:0x34]) != 0x4b78 or
                struct.unpack_from("<I", stub, 0x34)[0] not in HANDOFFS):
            raise ValueError("Unrecognized first-stage ARM7 source.")
        struct.pack_into("<I", stub, 0x34, handoff)
        for page in (1, 4, 5):
            claims["spare"].append((f"ARM7 copy page {page}", page * SPARE, bytes(stub)))
    if safe is not None:
        if len(safe) < 512 or not is_compact_safe_mode(safe[:512]):
            raise ValueError("Expected independently sourced compact safe mode.")
        used, = struct.unpack_from("<I", safe, 0x80)
        if not 512 <= used <= len(safe) or first < 0x18000 or first % CHUNK or first + used > 0x40000:
            raise ValueError("Unsafe size or unsupported contiguous safe-mode placement.")
        table = b"".join(struct.pack("<I", first + offset) for offset in range(0, used, CHUNK))
        claims["main"].append(("proposed contiguous primary entries", 0x8000, table))
        if first < pages * PAGE:
            claims["main"].append(("safe-mode ROM", first, safe[:pages * PAGE - first][:used]))
    return claims


def score(claims, held_out, length):
    if len(held_out) > length:
        held_out = held_out[:length]
    covered = bytearray(len(held_out))
    equal = compared = 0
    lines = []
    for label, offset, data in sorted(claims, key=lambda item: item[1]):
        n = max(0, min(len(data), len(held_out) - offset))
        if any(covered[offset:offset + n]):
            raise ValueError("Overlapping reconstruction claims.")
        covered[offset:offset + n] = b"\1" * n
        mismatches = [i for i in range(n) if data[i] != held_out[offset + i]]
        compared += n
        equal += n - len(mismatches)
        line = f"  {label} at 0x{offset:X}: {n - len(mismatches)}/{n} exact"
        if n != len(data):
            line += f" ({len(data) - n} bytes not captured)"
        if mismatches:
            line += f" (first difference at 0x{offset + mismatches[0]:X})"
        lines.append(line)
    missed = [(i, b) for i, b in enumerate(held_out) if not covered[i] and b != 0xff]
    lines.append(f"  {equal}/{compared} claimed bytes exact, {len(held_out) - compared} observed bytes unclaimed.")
    if missed:
        lines.append(f"  Filling unclaimed bytes with FF would miss {len(missed)} non-FF bytes, "
                     f"starting at 0x{missed[0][0]:X}.")
    if len(held_out) < length:
        lines.append(f"  0x{length - len(held_out):X} bytes not captured.")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("main", type=Path)
    parser.add_argument("spare", type=Path)
    parser.add_argument("--pages", type=int, choices=(64, 128), required=True)
    parser.add_argument("--prepared-stage1", type=Path)
    parser.add_argument("--logical-stage1", type=Path)
    parser.add_argument("--handoff", type=lambda v: int(v, 0), choices=HANDOFFS)
    parser.add_argument("--safe-rom", type=Path)
    parser.add_argument("--safe-address", type=lambda v: int(v, 0), default=0x20000)
    a = parser.parse_args()
    load = lambda p: p.read_bytes() if p else None
    try:
        claims = make_claims(a.pages, load(a.prepared_stage1), load(a.logical_stage1),
                             load(a.safe_rom), a.safe_address, a.handoff)
        print("Held-out block-0 audit. No output image or writes.")
        for label, path, width in (("main", a.main, PAGE), ("spare", a.spare, SPARE)):
            print(label.capitalize())
            with path.open("rb") as source:
                data = source.read(a.pages * width)
            print("\n".join(score(claims[label], data, a.pages * width)))
    except (OSError, ValueError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
