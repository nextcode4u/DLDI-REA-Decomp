#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    printf 'Usage: %s /path/to/clean-nrio-usb-disk /path/to/nrioTool\n' "$0" >&2
    exit 2
fi

mount_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
card=$(CDPATH= cd -- "$mount_dir/../.." && pwd)
driver="$card/unified/nrio-unified.dldi"
usb_checkout=$(CDPATH= cd -- "$1" && pwd)
nriotool=$(CDPATH= cd -- "$2" && pwd)

usb_revision=b9b781bf5deec5096cbc517432878430ac0e58c6
if [ "$(git -C "$usb_checkout" rev-parse HEAD)" != "$usb_revision" ]; then
    printf 'Check out nrio-usb-disk at %s before applying the patch.\n' "$usb_revision" >&2
    exit 2
fi
git -C "$usb_checkout" apply --unidiff-zero --check "$mount_dir/usb-app.patch"
git -C "$usb_checkout" apply --unidiff-zero "$mount_dir/usb-app.patch"
for file in msc.c volume.c volume.h; do
    cp "$mount_dir/$file" "$usb_checkout/source/"
done
for file in nand.c raw_nand.h; do
    cp "$card/raw-nand/source/$file" "$usb_checkout/source/"
done
make -C "$usb_checkout"
dlditool "$driver" "$usb_checkout/nrio-usb-disk.nds"

mkdir -p "$nriotool/data"
python3 - "$usb_checkout/nrio-usb-disk.nds" "$nriotool/data/usb_mount.bin" <<'PY'
import struct
import sys
from pathlib import Path

rom = bytearray(Path(sys.argv[1]).read_bytes())
signature = b"\xed\xa5\x8d\xbf Chishm\0"
offset = rom.find(signature)
if offset < 0 or rom.find(signature, offset + 1) >= 0 or rom[offset + 0x60:offset + 0x64] != b"NRIO":
    raise SystemExit("Expected exactly one embedded NRIO DLDI driver.")
source, _, _, size = struct.unpack_from("<4I", rom, 0x20)
if not (source <= offset < source + size and len(rom) <= 0x40000):
    raise SystemExit("The DLDI patch is outside ARM9 or the USB app exceeds the RAM slot.")

# Other launchers patch the surrounding nrioTool ROM. Keep this driver's
# descriptor for its packaged app without exposing a second DLDI magic.
rom[offset] = 0xee
identity = b"N-Card filesystem USB"
rom[offset + 0x10:offset + 0x40] = identity.ljust(0x30, b"\x00")
Path(sys.argv[2]).write_bytes(rom)
PY
