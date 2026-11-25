# nrioTool DSi SD recovery integration

This integration adds recovery options to Apache Thunder's nrioTool utility menu and links the same NAND backend used by the USB recovery toolkit. It uses nrioTool's existing DSi SD mount. The integration is based on [nrioTool commit `4cf70ae`](https://github.com/ApacheThunder/nrioTool/tree/4cf70ae13f908e2391cd608f833699d711a7d92a).

[Download the compiled nrioTool with native format, file browser, and USB mount](nrioTool.nds?raw=1).

## Dump SRLs

The Main Menu probes Slot-1 with the read-only NAND backend and lists the SRL dumps available for the detected layout. **Dump main SRL** saves the original 22-sector main area as `stage1.bin`. **Dump Safe Mode SRL**, **Dump bootleg SRL**, and **Dump uDisk SRL** follow their NAND block tables into `safe-original.nds`, `bootleg.nds`, or `udisk.nds`. The table reader recognizes a primary table redirected to uDisk and skips tables marked `QAFE`. Mapped dumps use 16 KiB blocks and retain the last block's padding. A credible ROM size limits the dump. Otherwise, the reader uses the valid prefix of the block table.

Files go to `sd:/nrioFiles` on DSi or `fat:/nrioFiles` in DS mode. Repeated safe-mode dumps get numbered filenames. For other dumps, move an existing file before repeating it. Press **B** during a dump to cancel. A completed file appears only after the read and close succeed.

## Menu controls

Use **Up/Down** to highlight a row, **A** to select it, and **B** to go back or exit. Choose **Utility Menu** from the Main Menu for cart checks, block-0 backups, and USB mounting or GodMode9Nrio where available. The read-only **Test NAND dump** is in the Main Menu. Choose **Advanced NAND tools** for boot-area writes and update conversion.

**Back up block 0** writes `main.bin` and packed `spare.bin` to the next available `sd:/nrioFiles/block0-NNN` directory.

The advanced menu includes these targeted fixes.

- **Patch safe mode for DSi.** The tool can prepare a patch from the cart's own stock safe-mode ROM. If `sd:/nrioFiles/safe-mode.nds` exists, it must match the cart's mapped ROM outside the header and ARM7 sectors and contain a recognized patch with the cart's original ARM7 payload. The tool accepts the toolkit patch and Apache's tested stub. Erased `FF` padding after the declared ROM length is accepted up to the next power-of-two size (at least 128 KiB). The patch still requires the live header and ARM7 sector to be in block 0.
- **Install custom banner.** Put the v1 banner in `sd:/nrioFiles/banner.bin`. Supply either the first `0x800` bytes or a complete `0x840`-byte banner with a zero-filled final `0x40` bytes. The tool verifies the CRC and replaces the banner page at physical NAND offset `0x2000`. The second copy at `0x2800` is preserved. The writer also preserves blank spare records and recognized copies of the ARM7 boot record. It regenerates ECC only if the existing triplets match the original main page.
- **Restore banner ARM7.** Repair page 4's spare record if the two intact ARM7 copies from pages 1 and 5 agree and the live page-4 damage matches the old banner writer's ECC pattern. The current banner and all other records are preserved. The tool saves a block-0 backup before confirmation.

The install and restore options save a block-0 backup to SD before asking for confirmation. The confirmation screen recommends keeping a backup from before the fault whenever possible. The reminder adds no backup requirement. Hold **L + R** and press **A** to write. The backend compares every live main and spare byte against its snapshot before erase, updates ECC where the selected operation calls for it, and verifies the full block after programming. Keep the SD backup for recovery if power or a NAND operation fails. Apache verified SD backup export and reported that the first banner build changed the displayed banner. This was a different cart from the verified DS Linker. It white-screened afterward and had no pre-write boot test. Apache suspected an existing uDisk problem. The backup showed that page 4's spare record duplicated the ARM7 boot code from pages 1 and 5. The revised banner path preserves that record in emulation and still needs a physical test. The ExpressCard's raw before-and-after dumps matched the old writer's output byte for byte. It changed 472 banner bytes and 34 ARM7 spare bytes, all on NAND page 4. The restore passed emulation on the actual post-write main and spare dumps and preserved the replacement banner. Apache confirmed on hardware that the ExpressCard booted again after the targeted ARM7 restore.

## DS and DS Lite recovery with Slot-2 storage

Launch `nrioTool.nds` from a Slot-2 storage device with its writable DLDI driver patched into the ROM. Put `nrioFiles` at the root of that device. The Utility Menu uses `fat:/nrioFiles` for backups and optional input files on this setup. In DSi mode it continues to use `sd:/nrioFiles`.

Hold **L** at launch to jump straight to recovery before the regular Slot-1 initialization. **Back up block 0** saves a block-0 backup from the N-Card in Slot-1. Recovery is available in DS mode when nrioTool mounts a read/write Slot-2 DLDI device. This avoids storing its backups on the Slot-1 cart being repaired. The Slot-2 path has been checked in software and still needs a physical DS or DS Lite test.

## Mount the N-Card over USB

Use a DS or DS Lite with the N-Card in Slot-1 and an N-Card USB writer in Slot-2. With the USB-enabled build of nrioTool, hold **X** at startup, or select **Mount USB drive** in the DS Utility Menu. Connect the writer to your computer. The cart's existing FAT volume appears as a removable drive, ready for copying ROMs and other files in either direction.

Eject the drive on the computer before pressing **START** on the DS to leave USB mode. The app requires a readable FAT volume and keeps raw NAND and recovery controls separate. The mount has been exercised with an emulated FAT volume. A physical DS and USB writer test is pending.

## DLDI Menu

The DLDI Menu still offers FAT dumps and sector-0 actions. The old default FAT-image writer was removed in favor of native format and menu setup. **Dump FAT sector 0** saves an exact 512-byte file as `nrio_fatsector0.bin` on independent storage without replacing an existing file. **Restore FAT sector 0** accepts a valid 512-byte FAT boot sector or FAT MBR. Use a dump from the same cart. Restore saves the current sector to `fat0-before-NNN.bin`, asks for **L + R + A** to confirm, rechecks before writing, and verifies the result after restarting the driver. Neither sector-0 action needs to mount the target FAT volume or switch to legacy DLDI mode.

You can also leave the DLDI Menu without changing modes. The legacy **Boot cart menu** and **Dump FAT image** actions still ask before switching modes. After the switch, **B** exits to the system menu.

## File Browser

Choose **File Browser** from the main menu. It starts in `nrioFiles` on the first mounted drive if that folder exists, or at the root otherwise. Press **B** to go up a folder. The DSi can show its SD card and the N-Card. On DS and DS Lite, the browser uses the mounted N-Card and, when available, a separate Slot-2 FAT device. If the N-Card cannot be mounted, other mounted drives remain available. The browser reports why and lets you retry the N-Card with L or R.

- Press **A** to open a folder or see a file's size. Use **Left/Right** to page the list and **L/R** to switch between the drive tabs. The browser remembers each drive’s folder and selection while it is open.
- Press **X** for Copy, Cut, Paste, Rename or Delete. After Copy or Cut, the queued file is marked red. Open the destination and choose Paste from Actions or press **Y**. Choose Cancel clipboard from Actions to clear the mark without changing the file. The bottom-screen paste preview shows where the files are going and the destination’s free space. On a filename conflict, choose **Skip**, **Keep both**, or **Replace**.
- Press **START** to make a folder. Rename and New Folder use a touch QWERTY keyboard. D-pad and A also work. Use X to delete, Y to change case, SELECT for symbols, and START to save. Folder deletion is limited to empty folders. Copy and Cut work on files, not whole folder trees.

Use **SELECT** to mark up to 32 files, then use Actions to Copy or Cut the batch. Transfers show the current file and total count, and report completed, skipped, failed, and remaining files when stopped. Cross-drive moves compare the destination with the original before removing it. The File Browser only uses mounted FAT filesystems. GodMode9Nrio remains an optional utility.

## uDisk setup on DSi and DS

On DSi, open **Utility Menu → Advanced NAND tools → uDisk setup**. On DS or DS Lite, hold **L** when launching nrioTool to reach setup without mounting the target cart. If the launcher has separate storage and the Utility Menu opens, take the same menu path as on DSi. Both format options require an SD launch path and a mounted DSi SD card, or a mounted Slot-2 DLDI. A Slot-1 or unidentified launch shows **Format unavailable**.

A single `nrioTool.nds` includes the stock uDisk 1.45 firmware image, the target cart driver, Apache Thunder’s file-based XuluMenu, and the stock XMenu starter files. You don't need a separate updater, formatter, or any setup files on SD.

- **Quick Format N-Card** erases the cart filesystem, respects the NAND bad-block list, and creates a clean FAT volume. It does not update boot firmware. This is Quick Format, not Full Format. The on-console option uses the detected 64- or 128-page geometry and checks that all active boot ROM mappings end before the filesystem metadata. This covers stock safe-mode/uDisk layouts and hidden XuluMenu layouts, including direct boot. It stops before erasing data when the boot maps cannot be verified or a physical map slot contains unexpected data.
- **Full Format N-Card** erases the same filesystem area and additionally programs and reads back a test pattern on every page of each eligible data block. It erases and verifies the block again, excludes failed blocks, and saves the updated bad-block bitmap before publishing the new FAT map. The completion screen separates tested blocks and new failures from the total skipped. This takes much longer than Quick Format, erases all cart files, and still leaves the boot firmware alone. Use Quick Format for routine rebuilds and Full Format when testing the NAND itself.
- **Install stock uDisk 1.45** updates the N-Card's hidden uDisk firmware, sometimes called its BIOS. It accepts the recognized stock 1.23 layout or a verified hidden XuluMenu release whose stock-size write window is contiguous. It still needs a safe-mode route, and requires it to be patched when run on a DSi. A cart already bypassing safe mode must restore that route before changing hidden firmware. If 1.45 is already installed, it skips the write. It leaves the console firmware, primary table, safe-mode ROM, and cart filesystem alone. The tool checks the original blocks, verifies readback and writes the secondary map last. Do not power off once writing starts. Writing the map last does not make the update power-loss safe.
- **Set up XuluMenu** is the default after formatting. It copies Apache Thunder’s file-based launcher to `xmenu.dat`, seeds the missing `DSYSTEM` files, and keeps stock XMenu as `xmenu.nds`. Launch `xmenu.nds` from the XuluMenu file browser for retail games. If an active `xmenu.dat` already exists, setup leaves it alone.
- **Switch to XuluMenu** explicitly changes an existing setup without formatting. It preserves the stock fallback and saves a different old launcher as `xmenu-old.nds`. It refuses to overwrite either backup path if it contains unrelated data. This option and fresh setup only write files on the cart, never the hidden uDisk firmware.
- **Copy stock/custom XMenu** copies missing files from the built-in starter without replacing existing files. To use a custom set instead, place `xmenu.dat` and the complete `DSYSTEM/` starter under `sd:/nrioFiles/` or on independent Slot-2 storage under `fat:/nrioFiles/`.
- **Delete old uDisk installer** is optional cleanup if a previous manual upgrade left the verified stock `autorun1.nds` on the cart. Stock uDisk will otherwise find that old installer before it finds the menu. Only the exact recognized file is removed. This never uninstalls hidden firmware. The native installer never creates that file.

Hold **L + R** and press **A** to confirm a destructive action. After formatting, run **Set up XuluMenu** before rebooting. The compressed starter files are embedded and streamed to the cart, so menu setup does not depend on NitroFS or a launch path. You do not need to format just to upgrade firmware. The menu-file setup uses the validated FAT mount and does not require a stock uDisk fingerprint. Launch the finished cart from the console's system menu.

If independent storage is available, setup saves logs and the firmware installer saves and checks affected erase-block backups there. Without it, the installer still checks the original blocks and write readback but cannot leave a backup or log after you exit.

On a physical 16 Gbit N-Card already running 1.45, native Quick Format and the previous stock XMenu starter produced a cart that booted from the DSi System Menu. Booting the bundled XuluMenu on a real cart, the DS setup path, and the native 1.23 → 1.45 upgrade still need hardware testing. See the [uDisk findings](../udisk/README.md#dsi-hardware-finding).

## Check the cart

Select **Check Cart** in the Utility Menu. The first page summarizes the likely failure. Use **Left/Right** for NAND and early boot data, both boot tables, and FAT/menu details. It distinguishes no NAND response from a fully erased block 0, identifies known uDisk revisions, and checks the FAT mount, `DSYSTEM`, `xmenu.dat`, and any higher-priority `autorun*.nds` launcher. nrioTool uses the same [C NRIO driver](../unified/README.md) for the File Browser and Check Cart. Both modes retry NAND ID detection and try later candidates if the physical map fails validation. Check Cart disables the write callback and does not perform NAND cleanup. A recognized header or successful mount does not establish that the cart will boot.

Press **L** for next steps based on the result. Press **R** to reprobe and rerun the report after reseating the cart. The FAT page distinguishes a missing marker, a rejected map record, a read failure, and a mount failure after finding a plausible map. No geometry is guessed and none of these checks write to the cart.

With independent storage, the report is appended to `nrioFiles/CHECK-CART.TXT` and **X** can verify a saved block-0 backup against the cart’s anchors. The report itself does not need an SD. To reach it with a cart that can’t boot, launch nrioTool from an independent device while holding **L**. If there is no external FAT mount, **Check Cart** appears in the startup uDisk setup menu. Repair and formatting stay separate and nothing is fixed automatically.

### NAND health scan

From **Check Cart**, press **Y** for a read-only quick scan. It reads the existing bad-block bitmap, checks every page in block 0, and samples the first and last pages of other erase blocks. Known bad data blocks are listed without being probed. The report separates bitmap entries, unusual spare data, sampled ECC differences, failed reads, and reads that recovered after retry. **X** opens physical NAND offsets, and you can recheck every page of a selected suspect block. **B** stops a scan without modifying the cart. When independent storage is available, it appends findings to `nrioFiles/NAND-HEALTH.TXT`.

A clean scan is not a program/erase test. This feature does not automatically retire or remap a block. A live repair still needs a verified spare, a full copy of the affected data, and a safe way to publish the updated map.

### Experimental 4 GiB address probe

Press **SELECT** in **Check Cart** to see the raw NAND ID and run the upper-address probe, including on a card whose ID does not match a supported normal geometry. Use the main page size from the exact NAND part number. The probe compares repeated 512-byte reads from pairs of rows 2 GiB apart. Distinct, repeatable upper data is evidence that those tested addresses work. Matching structured reads suggest mirroring, and uniform blank reads are inconclusive. The log is `nrioFiles/NAND-4GIB.TXT` when independent storage is available.

The probe does not mount, format, or change capacity. 4 GiB filesystem support still requires hardware confirmation and changes to geometry, address bounds, and the firmware and DLDI paths that use the map.

## Advanced NAND tools

Select **Advanced NAND tools** in the Utility Menu. The boot-region write paths validate the target, save affected erase blocks before confirmation, and verify every main and spare byte after rewriting a block. The backup reminder is advisory. These paths take their own snapshots and can start without an older backup.

- **Replace safe-mode ROM.** Put `safe-replacement.nds` in `nrioFiles` on the mounted storage. The tool reads and verifies the live safe-mode ROM. Any `safe-original.nds` on storage is a backup only and is not used to authorize the write. The replacement must fit in the existing mapping and pass header, ARM load range, and spare checks. Files up to 256 KiB are accepted. The checker keeps padding beyond the replacement's declared size on NAND and can stage changes in more than one erase block.
- **Bypass safe mode.** Redirects startup to the cart’s existing stock uDisk 1.45 or published hidden XuluMenu 1.3, 1.4, or 1.4.1 by copying the verified 16 KiB boot-table window from physical `0x40000` to `0x8000`. It verifies the mapped executable against the corresponding release before and after confirmation. It backs up the whole boot erase block in the normal **Restore block 0** format and preserves the safe-mode ROM, secondary map, and cart files. The safe-mode USB update screen is bypassed after this change. Unknown firmware and custom builds still fail the read-only check. This is a boot-block rewrite, not a firmware reinstall, and still needs testing on the FPGA DS Linker.
- **Repair stock uDisk.** Uses the same embedded 1.45 payload as the BIOS install. It requires an intact early safe-mode route and either the supported stock uDisk map or an erased secondary map over blank or already-matching sectors. It writes the secondary map last and leaves block 0 alone.

If **Replace safe-mode ROM** repeatedly misreads page `0xE` (`0x7000–0x77FF`) in the first erase block, it can offer to leave that page erased. The offer depends on the data and applies across supported cart models. Both reads must show all-`FF` spare bytes, an all-`FF` first half, and only sparse cleared bits in the second half. Press **A** to allow the exception or **B** to cancel. The backup retains the original read, and the usual **L + R + A** write confirmation still applies. The tool checks the erased-page pattern on readback and compares every other page exactly. Apache Thunder tested the page-`0xE` workaround on a DS Linker. The post-write backup matched the replacement ROM byte for byte. Page `0xE` read back entirely as `FF`, and the only other main and spare changes were the two patched sectors and their ECC. DSi boot for this cart remains unconfirmed.

The advanced actions append their diagnostic messages to `ADVANCED.TXT` and save `region-NNN/blocks.bin` and `region-NNN/SESSION.TXT` on the mounted storage. If a snapshot reread differs from the saved page, the tool checks up to four more times and proceeds only after two consecutive exact matches. It publishes `blocks.bin` only after the full backup passes preflight and closes successfully. A persistent mismatch reports the NAND page, whether it was in main or spare data, the byte offset, and both values. Each block snapshot includes every main and spare byte, its physical LBA, and a CRC. Use the integration project's extractor to verify the backup or extract main and packed spare files for each block:

```sh
python3 unpack_region.py region-000/blocks.bin --out extracted-blocks
```

After confirmation, the tool shows the current block and displays a separate completion or failure screen. A program or verification failure stops the operation and leaves the saved snapshots available. Stock uDisk repair still needs a physical-cart test.

Apache Thunder reported that a newer DS Linker safe-mode ROM could show its own screen on the older FPGA DS Linker but failed to hand off to uDisk. Reinstalling the FPGA cart’s own DSi-patched safe-mode ROM restored normal boot. That points to a cross-revision compatibility issue, not a demonstrated failure of the replacement writer.

### ExpressCard safe-mode sample

Apache Thunder supplied matching original and DSi-patched ExpressCard ROMs and the ARM7 stub source. The supplied assembly and linker script reproduce the 124-byte patched ARM7 payload, which copies the original 56-byte program from `0x02380000` to `0x03800000`. The stock ARM7 matches the in-place patcher's existing handoff fingerprint. The tool's patcher generates the same header changes but preserves the live cart's ARM7 body and banner.

Both samples are `0x15000` bytes with a declared length of `0x14E40`. Apache's replacement also changes the banner at ROM offset `0x14600`. It does not need a bigger block table on this cart, but does not establish compatibility with other ROM revisions. **Patch safe mode for DSi** edits the recognized live ROM; **Replace safe-mode ROM** is the separate full-image path. The ExpressCard sample ROMs are not bundled.

## Recover block 0 from a backup

While the cart is working, select **Back up block 0** in the Utility Menu to save a restore-ready backup. Keep `main.bin`, `spare.bin`, and `backup.info` together in their numbered folder. Backups from earlier builds lack the metadata used by this menu.

If the cart stops booting, launch nrioTool from the DSi SD card or Slot-2 storage while holding **L** to enter recovery before the regular cart initialization. The NAND bridge must still answer the raw probe. Choose **Advanced NAND tools**, then **Restore block 0**. Use **Left/Right** to choose the backup from that same cart, then press **A**. The app checks the file CRCs, NAND geometry, and saved data at `0x40000` and `0x80000`. Those data checks help catch a mismatched backup. Always use the original cart's backup.

After the app saves the current block to a separate backup folder, hold **L + R** and press **A** to restore. It checks the live snapshot before erase and verifies every main and spare byte after writing. This restores block 0 only. If block 0 is erased and there is no prior backup, the narrower ARM7 repair can help only when its required copies survive. The new startup shortcut and full restore still need a physical DSi test.

## Build

Apply this integration to a clean checkout of the pinned nrioTool commit.

```sh
git clone https://github.com/ApacheThunder/nrioTool.git
git -C nrioTool checkout 4cf70ae13f908e2391cd608f833699d711a7d92a
sh n-card/nrioTool/apply.sh nrioTool
make -C nrioTool LIBNDS=/path/to/legacy-sdk/opt/devkitpro/libnds
```

The build uses nrioTool's devkitARM Makefiles. Set `DEVKITPRO` and `DEVKITARM` and put the devkitPro tools on `PATH`. The local test build used devkitARM r67.1 with libnds 1.8.3 and libfat-nds 1.1.5 installed together in a separate SDK directory. nrioTool's ARM7 and bootloader use the pre-Calico FIFO and power-management APIs. [Archived libnds package](https://wii.leseratte10.de/devkitPro/libnds/libnds_1.8.3%20%282024-09-04%29/) · [Archived libfat package](https://wii.leseratte10.de/devkitPro/libfat/libfat_1.1.5%20%282020-04-18%29/)

The Makefile produces `nrioTool.nds`. Install Python 3 and devkitPro's `nds-zlib`. The build compiles the file-based XuluMenu in the upstream `XuluMenu/` folder and packs it with the stock 1.45 runtime, the stock menu starter, and the unified NRIO driver. XuluMenu source and license are in the upstream checkout. [Apache Thunder’s file-based XuluMenu source](https://github.com/ApacheThunder/nrioTool/tree/d1cdbae005d7656f66ab70a14d9ebdfe75f90cda/XuluMenu) No cart-specific NAND or spare data is embedded. The native upgrade uses the stock runtime as NAND data and does not run the stock updater or formatter. [Stock uDisk 1.45 package](https://linfoxdomain.com/nintendo/ds/dl/NCard_v1.45_v2.53_v25.zip)

To include USB mounting, run the companion build after applying the integration and before building nrioTool. Use a clean checkout of the USB app at the pinned revision, a BlocksDS installation, and `dlditool` on your PATH. The build patches the USB app with the same C driver packaged for nrioTool.

```sh
git clone https://github.com/ApacheThunder/nrio-usb-disk.git
git -C nrio-usb-disk checkout b9b781bf5deec5096cbc517432878430ac0e58c6
sh n-card/nrioTool/usb-mount/build.sh nrio-usb-disk nrioTool
make -C nrioTool LIBNDS=/path/to/legacy-sdk/opt/devkitpro/libnds
```

Keep `dlditool` on `PATH`. The companion build patches its USB checkout and stores the embedded ROM at `nrioTool/data/usb_mount.bin`. Use the regular nrioTool SDK for the final `make` command. [USB app source](https://github.com/ApacheThunder/nrio-usb-disk/tree/b9b781bf5deec5096cbc517432878430ac0e58c6)
