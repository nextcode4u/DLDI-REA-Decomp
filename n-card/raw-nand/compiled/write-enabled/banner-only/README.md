# Banner-only N-Card recovery

The [USB app template](nrio-raw-banner.nds) and [standalone DLDI template](nrio-raw-banner.dldi) ship unarmed. The USB app exposes the main NAND data read-only until you prepare a copy for a particular backup and replacement. Use the [read-only dump tools](../../../README.md) to save both the main and spare areas of the target before proceeding.

## Prepare from dumps

The [arming tool](../../../arm_banner.py) extracts the required bytes directly from separate main and spare dumps. Use Python 3 and a donor whose banner main page and spare record are compatible with the target boot layout. Put the template beside the Python script if downloading the files individually.

```sh
python3 arm_banner.py from-dumps target-spare.bin donor-main.bin donor-spare.bin
```

The command creates `banner-ready/nrio-raw-banner-armed.nds` and `banner-ready/banner.bin`. It reads the first 512 bytes from the target spare dump, 2,048 bytes at `0x2000` from the donor main dump, and 64 bytes at `0x100` from the donor spare dump. The driver uses those spare bytes verbatim, including ECC and boot metadata. The firmware's exact assignments within that record remain unconfirmed.

For custom banner data you can still supply pre-extracted slices with `--target-spare` (512 bytes), `--banner-main` (2,048 bytes), and `--banner-spare` (64 bytes). Run `python3 arm_banner.py --help` for the command syntax.

Boot the armed copy and submit `banner.bin` as one 2,048-byte USB write starting at byte offset `0x2000`. The DS rejects other ranges, sizes, or payloads. Before erasing, it compares all eight saved spare records against the target. It rewrites physical erase block 0 with the intended page 4 main and spare bytes, preserves the staged main and spare bytes of every other page, and verifies the entire block by reading it back.

The write still erases a boot-critical block. Loss of power or a hardware erase or program failure can leave the card unbootable. The banner-only build passed emulation on both erase-block geometries.

## Hardware result

Apache Thunder used the banner-only build to replace the first banner on a working N-Card. The DS menu displayed the new banner and the card still booted fully. Apache reported two banner copies in the region starting at `0x2000`. Changing the first copy was sufficient on the tested card. The purpose of the second copy and its behavior on other firmware remain unverified.
