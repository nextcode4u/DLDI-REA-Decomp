# Banner-only N-Card recovery

The [combined USB app](../nrio-raw-toolkit.nds) enables its banner mode after you prepare a copy for a particular target backup and replacement. Use the app's [read-only modes](../../../README.md#ds-mode-menu) to save both the main and spare areas of the target before proceeding. The previous [single-mode USB template](nrio-raw-banner.nds) and [single-mode DLDI](nrio-raw-banner.dldi) remain available.

## Prepare from dumps

The [arming tool](../../../arm_banner.py) extracts the required bytes directly from separate main and spare dumps. Use Python 3 and a donor whose banner main page and spare record are compatible with the target boot layout. It finds the combined app in the repository. If downloading files separately, pass `--template /path/to/nrio-raw-toolkit.nds`.

```sh
python3 arm_banner.py from-dumps target-spare.bin donor-main.bin donor-spare.bin
```

The command creates `banner-ready/nrio-raw-banner-armed.nds` and `banner-ready/banner.bin`. It reads the first 512 bytes from the target spare dump, 2,048 bytes at `0x2000` from the donor main dump, and 64 bytes at `0x100` from the donor spare dump. The driver uses those spare bytes verbatim, including ECC and boot metadata. The firmware's exact assignments within that record remain unconfirmed.

For custom banner data you can still supply pre-extracted slices with `--target-spare` (512 bytes), `--banner-main` (2,048 bytes), and `--banner-spare` (64 bytes). Run `python3 arm_banner.py --help` for the command syntax.

Boot the armed copy and select **Banner: prepared ROM only** from the combined app's menu. Hold **L + R** and press **A** to confirm. Submit `banner.bin` as one 2,048-byte USB write starting at byte offset `0x2000`. Banner mode rejects other ranges, sizes, or payloads. Before erasing, it compares all eight saved spare records against the target. It rewrites physical erase block 0 with the intended page 4 main and spare bytes, preserves the staged main and spare bytes of every other page, and verifies the entire block by reading it back. The previous single-mode app enters banner mode at launch when prepared.

The write still erases a boot-critical block. Loss of power or a hardware erase or program failure can leave the card unbootable. The banner-only build passed emulation on both erase-block geometries.

## Hardware result

Apache Thunder used the banner-only build to replace the first banner on a working N-Card. The DS menu displayed the new banner and the card still booted fully. Apache reported two banner copies in the region starting at `0x2000`. Changing the first copy was sufficient on the tested card. The purpose of the second copy and its behavior on other firmware remain unverified.


## Possible origin of the duplicate banner

Apache Thunder recalls that early cards were sold as DSFlash2 and used a USB dongle to flash a single game before uDisk. In the layout he described, the tool wrote the game at `0x40000`, updated the block table at `0x8000` to point to it, and copied the game's banner into the region starting at `0x2000`. His hypothesis is that the tool wrote a fixed `0x1000`-byte banner region and duplicated the banner to fill it. That explanation remains unverified. The hardware test shows that updating the first copy was sufficient for the tested card.
