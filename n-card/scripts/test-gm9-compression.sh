#!/bin/sh
set -eu

base=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat >"$tmp/test.c" <<'C'
#include "gm9-bundle.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    FILE *src = fopen(argv[1], "rb");
    FILE *dst = fopen(argv[2], "wb");
    if (!src || !dst) return 2;
    bool ok = nrio_unpack_gm9(src, dst);
    fclose(src);
    if (fclose(dst)) ok = false;
    return ok ? 0 : 1;
}
C

"${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"$base/nrioTool" "$tmp/test.c" \
    "$base/nrioTool/gm9-bundle.c" -lz -o "$tmp/test"

python3 -B - "$base/nrioTool" "$tmp" "${1:-}" <<'PY'
from pathlib import Path
import random
import sys

sys.path.insert(0, sys.argv[1])
from prepare_nitrofs import stage

out = Path(sys.argv[2])
original = Path(sys.argv[3]).read_bytes() if sys.argv[3] else random.Random(831).randbytes(688640)
inputs = out / "nrioFiles"
inputs.mkdir()
(inputs / "GM9Nrio.nds").write_bytes(original)
(inputs / "other.bin").write_bytes(b"other NitroFS file")
stage(out)
files = out / "build" / "nitrofs"
assert not (files / "GM9Nrio.nds").exists()
assert (files / "other.bin").read_bytes() == b"other NitroFS file"
assert (inputs / "GM9Nrio.nds").read_bytes() == original
packed = (files / "GM9Nrio.z").read_bytes()
bad = {
    "bad-crc": packed[:12] + bytes([packed[12] ^ 1]) + packed[13:],
    "bad-size": packed[:8] + bytes(4) + packed[12:],
    "truncated": packed[:-4],
    "trailing": packed + b"x",
    "bad-tag": b"bad!" + packed[4:],
}
for name, data in bad.items():
    (out / name).write_bytes(data)
PY

"$tmp/test" "$tmp/build/nitrofs/GM9Nrio.z" "$tmp/decoded.nds"
cmp "$tmp/nrioFiles/GM9Nrio.nds" "$tmp/decoded.nds"
for bad in bad-crc bad-size truncated trailing bad-tag; do
    result=0
    "$tmp/test" "$tmp/$bad" "$tmp/rejected.nds" || result=$?
    test "$result" -eq 1
done
printf '%s\n' 'GodMode9Nrio decoded byte for byte and rejected all malformed streams.'
