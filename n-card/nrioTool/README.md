# nrioTool DSi SD recovery integration

This integration adds recovery options to Apache Thunder's nrioTool utility menu and links the same NAND backend used by the USB recovery toolkit. It uses nrioTool's existing DSi SD mount. The integration is based on [nrioTool commit `4cf70ae`](https://github.com/ApacheThunder/nrioTool/tree/4cf70ae13f908e2391cd608f833699d711a7d92a).

## DSi utility menu

- **Y — Install DSi safe mode.** The tool can prepare a patch from the cart's own stock safe-mode ROM. If `sd:/nrioFiles/safe-mode.nds` exists, it must match the cart's mapped ROM outside the header and ARM7 sectors and contain a recognized patch with the cart's original ARM7 payload. The tool accepts the toolkit patch and Apache's tested stub.
- **R — Install SD banner.** Put the v1 banner in `sd:/nrioFiles/banner.bin`. Supply either the first `0x800` bytes or a complete `0x840`-byte banner with a zero-filled final `0x40` bytes. The tool verifies the CRC and replaces the banner page at physical NAND offset `0x2000`. The second copy at `0x2800` is preserved.
- **SELECT — Back up block 0.** Writes `main.bin` and packed `spare.bin` to the next available `sd:/nrioFiles/block0-NNN` directory.

Both install options save a block-0 backup to SD before asking for confirmation. Hold **L + R** and press **A** to write. The backend compares every live main and spare byte against its snapshot before erase, regenerates ECC for the changed sectors, and verifies the full block after programming. Keep the SD backup for recovery if power or a NAND operation fails. The SD workflow still needs a physical DSi test.

## Build

Apply this integration to a clean checkout of the pinned nrioTool commit.

```sh
git clone https://github.com/ApacheThunder/nrioTool.git
git -C nrioTool checkout 4cf70ae13f908e2391cd608f833699d711a7d92a
sh n-card/nrioTool/apply.sh nrioTool
make -C nrioTool LIBNDS=/path/to/legacy-sdk/opt/devkitpro/libnds
```

The build uses nrioTool's devkitARM Makefiles. Set `DEVKITPRO` and `DEVKITARM` and put the devkitPro tools on `PATH`. The local test build used devkitARM r67.1 with libnds 1.8.3 and libfat-nds 1.1.5 installed together in a separate SDK directory. nrioTool's ARM7 and bootloader use the pre-Calico FIFO and power-management APIs. [Archived libnds package](https://wii.leseratte10.de/devkitPro/libnds/libnds_1.8.3%20%282024-09-04%29/) · [Archived libfat package](https://wii.leseratte10.de/devkitPro/libfat/libfat_1.1.5%20%282020-04-18%29/)

The Makefile produces `nrioTool.nds`. The integration adds no cart-specific NAND or spare data to the ROM.
