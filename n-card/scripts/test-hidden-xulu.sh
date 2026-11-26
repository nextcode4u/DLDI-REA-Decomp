#!/bin/sh
set -eu
if [ "$#" -ne 3 ] && [ "$#" -ne 5 ] && [ "$#" -ne 6 ] && [ "$#" -ne 7 ]; then
    echo "Usage: $0 nrio_bundle.bin patched-safe-mode.nds xulumenu.bin [ExpressCard-main.bin ExpressCard-spare.bin | --archive udisk.nds | --fire fire16-main.bin fire8-main.bin | --fpga main.bin spare.bin backup.info]" >&2
    exit 2
fi
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cp "$1" "$tmp/bundle.bin"
cp "$2" "$tmp/patched.nds"
cp "$3" "$tmp/xulumenu.bin"
if [ "$#" -eq 5 ]; then
    if [ "$4" = --archive ]; then
        cp "$5" "$tmp/older-udisk.bin"
    else
        cp "$4" "$tmp/express-main.bin"
        cp "$5" "$tmp/express-spare.bin"
    fi
fi
if [ "$#" -eq 6 ]; then
    test "$4" = --fire
    cp "$5" "$tmp/fire16-main.bin"
    cp "$6" "$tmp/fire8-main.bin"
fi
if [ "$#" -eq 7 ]; then
    test "$4" = --fpga
    cp "$5" "$tmp/fpga-main.bin"
    cp "$6" "$tmp/fpga-spare.bin"
    cp "$7" "$tmp/fpga-backup.info"
fi
cd "$tmp"
python3 - <<'PY'
from pathlib import Path
tagged = Path("xulumenu.bin").read_bytes()
assert len(tagged) == 2048 * 515
Path("xulu141.nds").write_bytes(b"".join(tagged[i:i+512] for i in range(0, len(tagged), 515)))
PY
cat >nds.h <<'C'
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
#define BUS_OWNER_ARM9 0
static inline void sysSetCardOwner(int owner) { (void)owner; }
static inline bool isDSiMode(void) { return true; }
C
cat >bundle.S <<'ASM'
.section .rodata
.global nrio_bundle_bin
.global nrio_bundle_bin_end
nrio_bundle_bin:
.incbin "bundle.bin"
nrio_bundle_bin_end:
.section .note.GNU-stack,"",@progbits
ASM
# Match the include layout used inside the application.
ln -s "$base/raw-nand/source" recovery_nand
ln -s "$base/udisk" udisk
src="$base/nrioTool"
raw="$base/raw-nand/source"
"${HOST_CC:-cc}" -O2 -ffunction-sections -fdata-sections -I. -I"$src" \
    "$src/tests/hidden-xulu-test.c" bundle.S "$src/region-recovery.c" \
    "$src/bundle-assets.c" "$src/udisk-upgrade-check.c" "$raw/recovery.c" "$raw/restore.c" \
    udisk/updater.c udisk/payload.c udisk/runtime.c -Wl,--gc-sections -lz -o test-hidden-xulu
if [ "$#" -eq 5 ] && [ "$4" = --archive ]; then
    ./test-hidden-xulu --archive older-udisk.bin
elif [ "$#" -eq 7 ]; then
    ./test-hidden-xulu --fpga fpga-main.bin fpga-spare.bin fpga-backup.info
elif [ "$#" -eq 6 ]; then
    ./test-hidden-xulu --fire fire16-main.bin fire8-main.bin
elif [ "$#" -eq 5 ]; then
    ./test-hidden-xulu express-main.bin express-spare.bin
else
    ./test-hidden-xulu
fi
