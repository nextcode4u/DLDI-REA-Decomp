# nrioTool DSi SD recovery integration

This integration adds recovery options to Apache Thunder's nrioTool utility menu and links the same NAND backend used by the USB recovery toolkit. It uses nrioTool's existing DSi SD mount. The integration is based on [nrioTool commit `4cf70ae`](https://github.com/ApacheThunder/nrioTool/tree/4cf70ae13f908e2391cd608f833699d711a7d92a).

## DSi utility menu

- **Y — Install DSi safe mode.** The tool can prepare a patch from the cart's own stock safe-mode ROM. If `sd:/nrioFiles/safe-mode.nds` exists, it must match the cart's mapped ROM outside the header and ARM7 sectors and contain a recognized patch with the cart's original ARM7 payload. The tool accepts the toolkit patch and Apache's tested stub.
- **R — Install SD banner.** Put the v1 banner in `sd:/nrioFiles/banner.bin`. Supply either the first `0x800` bytes or a complete `0x840`-byte banner with a zero-filled final `0x40` bytes. The tool verifies the CRC and replaces the banner page at physical NAND offset `0x2000`. The second copy at `0x2800` is preserved. The writer also preserves blank spare records and recognized copies of the ARM7 boot record. It regenerates ECC only if the existing triplets match the original main page.
- **SELECT — Back up block 0.** Writes `main.bin` and packed `spare.bin` to the next available `sd:/nrioFiles/block0-NNN` directory.
- **UP — Restore banner ARM7.** Repair page 4's spare record if the two intact ARM7 copies from pages 1 and 5 agree and the live page-4 damage matches the old banner writer's ECC pattern. The current banner and all other records are preserved. The tool saves a block-0 backup before confirmation.

The install and restore options save a block-0 backup to SD before asking for confirmation. Hold **L + R** and press **A** to write. The backend compares every live main and spare byte against its snapshot before erase, updates ECC where the selected operation calls for it, and verifies the full block after programming. Keep the SD backup for recovery if power or a NAND operation fails. Apache verified SD backup export and reported that the first banner build changed the displayed banner. This was a different cart from the verified DS Linker. It white-screened afterward and had no pre-write boot test. Apache suspected an existing uDisk problem. The backup showed that page 4's spare record duplicated the ARM7 boot code from pages 1 and 5. The revised banner path preserves that record in emulation and still needs a physical test. The ExpressCard's raw before-and-after dumps matched the old writer's output byte for byte. It changed 472 banner bytes and 34 ARM7 spare bytes, all on NAND page 4. The restore passed emulation on the actual post-write main and spare dumps and preserved the replacement banner. Apache confirmed on hardware that the ExpressCard booted again after the targeted ARM7 restore.

## Recover block 0 from the DSi SD card

While the cart is working, press **SELECT** in the Utility Menu to save a restore-ready backup. Keep `main.bin`, `spare.bin`, and `backup.info` together in their numbered folder. Backups from earlier builds lack the metadata used by this menu.

If the cart stops booting, launch nrioTool from the DSi SD card while holding **L** to enter recovery before the regular cart initialization. The NAND bridge must still answer the raw probe. Press **Down**, use **Left/Right** to choose the backup from that same cart, then press **A**. The app checks the file CRCs, NAND geometry, and saved data at `0x40000` and `0x80000`. Those data checks help catch a mismatched backup. Always use the original cart's backup.

After the app saves the current block to a separate SD folder, hold **L + R** and press **A** to restore. It checks the live snapshot before erase and verifies every main and spare byte after writing. This restores block 0 only. If block 0 is erased and there is no prior backup, the narrower ARM7 repair can help only when its required copies survive. The new startup shortcut and full restore still need a physical DSi test.

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
