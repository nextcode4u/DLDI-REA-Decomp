# NRIO DLDI reconstruction

The standalone build matches all 19,724 bytes of XuluMenu's `nrio.dldi`. It covers the DLDI interface, Slot-1 command transport, NAND access, and flash translation layer. The working source currently contains 40 recovered C routines and 20 ARM assembly routines. The bulk transfers with ECC generation and most of the translation layer still use assembly. The devkitARM startup and unsigned division code are linked from the historical toolchain. [Pinned XuluMenu reference](https://github.com/ApacheThunder/XuluMenu/blob/7ceabcbc9331f84a8c4fcde38b6f60193405ee8a/nrio.dldi)

## Build and compare

Use devkitARM r20 with its GCC 4.1.1, binutils, and an era-compatible libnds installed beside it in the standard devkitPro layout. The build includes the SDK's original `ds_rules` and uses `<nds.h>` for the SDK types and card register definitions. The linker pulls in `libgcc` support. libfat is a consumer of the DLDI interface, so there is no libfat link dependency. The Win32 r20 tools were tested under WSL. [Archived devkitARM r20](https://wii.leseratte10.de/devkitPro/devkitARM/r20%20%282007%29/)

```sh
make DEVKITPRO=/path/to/r20-sdk
make DEVKITPRO=/path/to/r20-sdk verify
```

In this workspace the existing SDK root is `/mnt/d/WORKSPACES/jenesisds-r20-sdk.A6WpYa`. The build writes `build/nrio.dldi`, `build/nrio.elf`, and `build/nrio.map`. The reference binary is read only by `make verify`, which compares every byte with `cmp`.

Expected SHA-256

```text
716937fa83dda22df3b27bf2c08ce9ea9a0209c6d64498a83b527c0dddae8978
```

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

Run from the source directory with `rea` on `PATH` and `GHIDRA_INSTALL_DIR` set to the Ghidra installation.

```sh
mkdir -p build/ghidra
"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$PWD/build/ghidra" nrio \
  -import "$PWD/build/nrio.elf" -scriptPath "$PWD/scripts/ghidra" -preScript MarkCardIo.java
REA_GHIDRA_PROJECT_PATH="$PWD/build/ghidra/nrio.gpr" \
  rea decompile-all "$PWD/build/nrio.elf" --provider ghidra \
  --address-start 0xbf800080 --address-end 0xbf804bab --output "$PWD/build/rea"
```

These commands create a fresh Ghidra project. Use fresh output directories for an independent rerun. [morluto/rea](https://github.com/morluto/rea)
