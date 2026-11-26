#!/bin/sh
# Tests the diagnostic classifier and SRL dump planning against a user's archive.
set -eu
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 udisk.nds 'archive name' hexadecimal-used-size" >&2
    exit 2
fi
base=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
ln -s "$base/raw-nand/source" "$tmp/recovery_nand"
src="$base/nrioTool"
"${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections \
    -I"$tmp" -I"$src" "$src/tests/archived-firmware-test.c" "$src/udisk-upgrade-check.c" \
    "$src/srl-dump.c" -Wl,--gc-sections -o "$tmp/check"
"$tmp/check" "$@" 8000
"$tmp/check" "$@" 40000
