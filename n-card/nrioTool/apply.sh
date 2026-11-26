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
git -C "$checkout" apply "$integration/udisk-setup.patch"
git -C "$checkout" apply "$integration/file-browser.patch"
git -C "$checkout" apply "$integration/safe-live.patch"
git -C "$checkout" apply "$integration/clipboard-ui.patch"
git -C "$checkout" apply "$integration/xulumenu-setup.patch"
git -C "$checkout" apply "$integration/bypass-safe-mode.patch"
git -C "$checkout" apply "$integration/boot-report.patch"
git -C "$checkout" apply "$integration/advanced-menu-format.patch"
git -C "$checkout" apply "$integration/lazy-dldi-menu.patch"
git -C "$checkout" apply "$integration/unified-dldi.patch"
git -C "$checkout" apply "$integration/full-format.patch"
git -C "$checkout" apply "$integration/retire-fat-image.patch"
git -C "$checkout" apply "$integration/fat-sector0.patch"
git -C "$checkout" apply "$integration/xulu-native.patch"
git -C "$checkout" apply "$integration/boot-routes.patch"
git -C "$checkout" apply "$integration/main-marker.patch"
git -C "$checkout" apply "$integration/compressed-gm9.patch"
mkdir -p "$checkout/arm9/source/recovery_nand" "$checkout/arm9/source/card" "$checkout/arm9/source/udisk"
for file in nand.c raw_nand.h recovery.c recovery.h restore.c banner_plan.h; do
    cp "$recovery/$file" "$checkout/arm9/source/recovery_nand/"
done
cp "$integration/../source/card/nand_ecc.h" "$checkout/arm9/source/card/"
for file in sd-recovery.c block0-backup-check.h region-recovery.c region-recovery.h srl-dump.c srl-dump.h quick-format.c \
            udisk-setup.c udisk-setup.h udisk-setup-io.c udisk-setup-io.h \
            udisk-upgrade-check.c udisk-upgrade-check.h bundle-assets.c bundle-assets.h gm9-bundle.c gm9-bundle.h cart-dldi.c cart-dldi.h sector-zero.c sector-zero.h sector-zero-ui.c file-browser.c file-browser.h \
            file-transfer.c file-transfer.h name-editor.c name-editor.h button-legend.h xulu-install.c xulu-install.h boot-report.c boot-report.h nand-health.c nand-health.h nand-health-ui.c nand-capacity-probe.c nand-capacity-probe.h nand-capacity-ui.c format-origin.h; do
    cp "$integration/$file" "$checkout/arm9/source/"
done
for file in updater.c updater.h payload.c runtime.c runtime.h format-native.c format-native.h; do
    cp "$integration/../udisk/$file" "$checkout/arm9/source/udisk/"
done
cp "$integration/usb-mount/ram-launch.h" "$checkout/bootloader/source/"
cp "$integration/embed_bundle.py" "$integration/prepare_nitrofs.py" "$checkout/"
mkdir -p "$checkout/assets"
cp "$integration/assets/starter.zip" "$checkout/assets/"
rm -f "$checkout/nrioFiles/nrio_basicimage_1995712_16g.img" \
      "$checkout/nrioFiles/nrio_basicimage_2011584_16g.img"
cp "$integration/../unified/nrio-unified.dldi" "$checkout/"
