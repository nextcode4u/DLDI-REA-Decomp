# NRIO DLDI reconstruction

The default build compiles all 60 N-Card driver routines from C, covering the DLDI interface, Slot-1 command transport, NAND access with ECC, and flash translation layer. The optional matching build reproduces all 19,724 bytes of XuluMenu's `nrio.dldi` using 52 C routines and eight assembly routines. Both builds use an assembly DLDI header and data tables, together with startup and division code from the historical devkitARM toolchain. [Pinned XuluMenu reference](https://github.com/ApacheThunder/XuluMenu/blob/7ceabcbc9331f84a8c4fcde38b6f60193405ee8a/nrio.dldi)

## Build and compare

Use devkitARM r20 with its GCC 4.1.1, binutils, and an era-compatible libnds installed beside it in the standard devkitPro layout. The build includes the SDK's original `ds_rules` and uses `<nds.h>` for the SDK types and card register definitions. The linker pulls in `libgcc` support. libfat is a consumer of the DLDI interface, so there is no libfat link dependency. The Win32 r20 tools were tested under WSL. [Archived devkitARM r20](https://wii.leseratte10.de/devkitPro/devkitARM/r20%20%282007%29/)

```sh
make DEVKITPRO=/path/to/r20-sdk
```

In this workspace the SDK root is `/mnt/d/WORKSPACES/jenesisds-r20-sdk.A6WpYa`. The default build writes `build/nrio.dldi`, `build/nrio.elf`, and `build/nrio.map`. The checked-in C binary and comparison report are in [compiled-from-c](compiled-from-c/).

## Byte-matching build

```sh
make DEVKITPRO=/path/to/r20-sdk C_ONLY=0 verify
```

This builds `build/matching/nrio.dldi` and compares it with the reference using `cmp`. With a successful comparison, the expected SHA-256 is

```text
716937fa83dda22df3b27bf2c08ce9ea9a0209c6d64498a83b527c0dddae8978
```

Byte matching from C remains for the two ECC transfers, page operation dispatch, NAND page copying, geometry initialization, zone loading, and the two mapped sector operations.

## Compare driver behavior

After building both configurations, run the ARM926 differential checks with Python and Unicorn 2.1.4 installed.

```sh
python3 scripts/check_c_behavior.py
```

The checks compare function results, driver state, buffer writes, and Slot-1 traffic with scripted NAND I/O. They cover 416 scenarios each at the original and relocated load addresses, including the public startup, read, and write entry points. The write transport checks add 16 scenarios at each address. They exercise the public sector-write entry point with simulated Slot-1 FIFO stalls, NAND-ready polling, and both erased and previously mapped blocks. They compare Slot-1 traffic and verify the requested sectors reach the FIFO and NAND program commands complete. To run these cases alone, use `python3 scripts/check_c_behavior.py --write-transport-only`.

Apache Thunder tested the C build on a physical N-Card using GodMode9i. It mounted the card, deleted the benchmark's pad file, and copied a file between folders successfully. The benchmark reported about 4.2 MB/s for sequential and random 2 MiB reads. Its write test stalled with both the C build and the stock driver. [Hardware test notes](compiled-from-c/README.md#hardware-test)

## Physical NAND recovery

The [raw NAND recovery builds](raw-nand/) start at physical sector 0 and bypass the flash translation layer. See the [recovery instructions](raw-nand/#recovery-instructions) for imaging and choosing a repair path, and the [hardware findings](raw-nand/#hardware-results) for Apache Thunder’s test results.

## Source tree

| Directory | Contents |
| --- | --- |
| `source/dldi` | DLDI header and six public entry points |
| `source/card` | Slot-1 command framing in C, plus bulk NAND data transfers and ECC generation |
| `source/nand` | ECC correction, spare-area operations, and page selection |
| `source/ftl` | Geometry, logical-to-physical mapping, block allocation, state, and sector I/O |
| `source/system` | Original strings and position-independent global offset table |
| `linker` | Original DLDI address layout and source section order |

## Binary and protocol evidence

- XuluMenu's driver matches the drivers published by nrioTool and DS-Homebrew byte for byte. The XMenu 2.55 `xmenu.dat` in nrioTool's stock image contains the same 19,724-byte driver at file offset `0xD272C`. In that copy, the feature byte at `0x64` is `0x21`, advertising reads only. XuluMenu sets it to `0x23`, also advertising writes. XuluMenu identifies its driver as compatible with uDisk 1.45. The copies establish compatibility and provenance, and the final historical release date remains unverified. [XuluMenu README](https://github.com/ApacheThunder/XuluMenu/blob/7ceabcbc9331f84a8c4fcde38b6f60193405ee8a/README.md) · [nrioTool stock image](https://github.com/ApacheThunder/nrioTool/blob/66fe785c0e71366cd79647078763072fd12cfce7/nrioFiles/nrio_basicimage_1995712_16g.img) · [DS-Homebrew NRIO staging](https://github.com/DS-Homebrew/DLDI/tree/3d3ab984a718e9be9e0af7805ae3ef2277bef7a1/staging/nrio)
- The original startup, constructor support, and GCC unsigned division objects match devkitARM r20. The recovered C matches using ARMv4T code generation, ARM7TDMI scheduling, `-O2`, and `-fPIC`. This is consistent with a 2007-era devkitARM build. [devkitARM r19 release announcement and GCC lineage](https://sourceforge.net/p/devkitpro/news/2006/05/devkitarm-release-19/)
- The driver sends standard NAND opcodes in proprietary Slot-1 frames. For example, `D1 61 90` requests the NAND ID, `D1 61 00` and `D1 61 30` bracket a read, and `D1 61 60` and `D1 61 D0` bracket an erase. Transfers use the card command registers, ROMCTRL, and the Slot-1 FIFO. The setup also writes the AUXSPICNT high byte to enable the card interface and its completion IRQ. [nrioTool cartridge commands](https://github.com/ApacheThunder/nrioTool/blob/4cf70ae13f908e2391cd608f833699d711a7d92a/arm9/source/nrio_card.c)

## REA analysis

REA 6.9.0 with Ghidra 12.1.4 was used to recover control flow and cross-check the reconstructed C against the ARM instructions. The included Ghidra script marks Slot-1 I/O regions volatile so the decompiler preserves polling loops and FIFO reads.

Build the matching configuration for the original routine addresses. Run from the source directory with `rea` on `PATH` and `GHIDRA_INSTALL_DIR` set to the Ghidra installation.

```sh
mkdir -p build/ghidra
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PWD/build/ghidra" nrio \
  -import "$PWD/build/matching/nrio.elf" -scriptPath "$PWD/scripts/ghidra" -preScript MarkCardIo.java
REA_GHIDRA_PROJECT_PATH="$PWD/build/ghidra/nrio.gpr" \
  rea decompile-all "$PWD/build/matching/nrio.elf" --provider ghidra \
  --address-start 0xbf800080 --address-end 0xbf804bab --output "$PWD/build/rea"
```

These commands create a fresh Ghidra project. Use fresh output directories for an independent rerun. [morluto/rea](https://github.com/morluto/rea)
