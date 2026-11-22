#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    printf 'Usage: %s /path/to/nrioTool-checkout\n' "$0" >&2
    exit 2
fi

integration=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
recovery="$integration/../raw-nand/source"
checkout=$(CDPATH= cd -- "$1" && pwd)

git -C "$checkout" apply --check --unidiff-zero "$integration/nriotool.patch"
git -C "$checkout" apply --unidiff-zero "$integration/nriotool.patch"
mkdir -p "$checkout/arm9/source/recovery_nand" "$checkout/arm9/source/card" "$checkout/arm9/source/udisk"
for file in nand.c raw_nand.h recovery.c recovery.h restore.c banner_plan.h; do
    cp "$recovery/$file" "$checkout/arm9/source/recovery_nand/"
done
cp "$integration/../source/card/nand_ecc.h" "$checkout/arm9/source/card/"
for file in sd-recovery.c region-recovery.c region-recovery.h; do
    cp "$integration/$file" "$checkout/arm9/source/"
done
for file in updater.c updater.h payload.c; do
    cp "$integration/../udisk/$file" "$checkout/arm9/source/udisk/"
done
