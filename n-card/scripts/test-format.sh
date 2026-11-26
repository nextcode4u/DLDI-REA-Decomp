#!/bin/sh
# Host-only regressions. Build the unified DLDI before running.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
export PYTHONDONTWRITEBYTECODE=1
cc=${HOST_CC:-cc}
python=${PYTHON:-python3}
driver=${1:-build/unified/nrio.elf}
test -f "$driver"
"$cc" -std=c11 -shared -fPIC -Wall -Wextra -Werror \
    udisk/format-native.c udisk/runtime.c udisk/payload.c nrioTool/sector-zero.c -o "$tmp/format.so"
"$cc" -std=c11 -Wall -Wextra -Werror nrioTool/tests/format-origin-test.c -o "$tmp/format-origin"
"$cc" -std=gnu11 -Wall -Wextra -Werror nrioTool/tests/xulu-install-test.c \
    nrioTool/xulu-install.c nrioTool/udisk-setup-io.c -lz -o "$tmp/xulu-install"
"$tmp/xulu-install"
"$tmp/format-origin"
"$cc" -std=gnu11 -Wall -Wextra -Werror nrioTool/tests/block0-backup-check-test.c -o "$tmp/block0-backup-check"
"$tmp/block0-backup-check"
"$cc" -std=c11 -Wall -Wextra -Werror nrioTool/tests/sector-zero-test.c nrioTool/sector-zero.c -o "$tmp/sector-zero"
"$tmp/sector-zero"
"$cc" -std=c11 -Wall -Wextra -Werror nrioTool/tests/boot-fingerprint-test.c \
    nrioTool/udisk-upgrade-check.c -o "$tmp/boot-fingerprint"
"$tmp/boot-fingerprint"
"$python" udisk/test_native_format.py "$tmp/format.so"
"$python" udisk/test_full_format.py "$tmp/format.so"
"$python" experimental/tests/test_driver.py "$driver"
"$python" unified/test_format_roundtrip.py "$driver" "$tmp/format.so"
"$python" unified/test_bus_wait.py "$driver" "$tmp/format.so"
