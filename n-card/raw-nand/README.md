# N-Card physical NAND recovery

## N-Card Recovery Toolkit

Use one [N-Card Recovery Toolkit `.nds`](compiled/write-enabled/nrio-raw-toolkit.nds) for main-area and spare-area access. It starts with main-area read-only selected and links the recovery backend directly. The [standalone raw DLDI](compiled/write-enabled/nrio-raw-toolkit.dldi) exposes physical main-area reads through the standard DLDI interface. For filesystem access through libfat, use the [NRIO filesystem driver](../compiled-from-c/).

### DS mode menu

At the first screen, make sure the target N-Card is in Slot-1 and press **A** to detect it. If detection fails, press **A** to retry or **B** to finish and export the log. The toolkit opens the mode menu after detecting the NAND.

On a DS or DS Lite with the Slot-2 USB writer, choose a mode with **Up/Down**, then press **A**. To enable a write mode, hold **L + R** and press **A** again. Connect USB after selecting. Eject the disk on the PC and disconnect the USB cable before pressing **START**. The results screen offers **A: export log**, **B: modes**, and **START: exit**. Returning to modes reprobes Slot-1 and resets NAND mode, write status, and USB transfer counters. You can switch views and run on-console checks in one launch.

| Mode | Operation |
| --- | --- |
| Main: read only | Dump physical NAND main data |
| Spare: read only | Dump packed 64-byte spare records |
| Main: protect boot block | Write main data outside the first physical erase block |
| Main: full recovery write | Write main data, including the first block, subject to preflight |
| Spare: recovery write | Write packed spare records, subject to preflight |
| Banner: prepared ROM only | Write the prepared page 4 payload after checking the target spare snapshot |
| Repair primary boot table | On-console repair using the target cart's surviving uDisk mapping |
| Install stock safe mode | Prepare and install the DSi patch on console, or install from a desktop-prepared snapshot |
| Check recovery only | Run the same preflight with read-only NAND access and export the result |
| Check safe mode | Detect the stock ARM7 layout or a recognized DSi relocation patch using read-only NAND access |

The older single-mode binaries used in the hardware reports are available in the [archived revision](https://github.com/nextcode4u/DLDI-REA-Decomp/tree/0b09821d66c6e91f2a94383f055720258bcfd1fb/n-card/raw-nand/compiled).

### Save a DS session log

The screen reports cart detection, NAND geometry, repair checks, the first USB read and write, progress at each additional 8 MiB, and failures with their LBA or NAND page. A USB NAND session ends with read, write, and error counts. Table repair reports which checks passed or where it stopped.

When **A: export log** appears after a check, repair, or USB NAND session, press **A** and connect the Slot-2 USB writer to the PC. Copy `SESSION.TXT` from the **NRIO LOGS** drive. This read-only drive is generated in DS memory and uses a separate USB identity. Eject it and unplug USB before pressing **START** to return to the results screen. The log accumulates across modes during the launch. USB re-enumeration between modes still needs a hardware test.

### Repair using only the target cart

1. Save main and spare backups from the target with the read-only modes. Launch the toolkit independently of the target cart if it cannot boot.
2. With the USB cable disconnected, choose **Check recovery only**. The DS checks the early boot data, shared spare records, secondary table at `0x40000`, and the uDisk image it maps. It reports how many table sectors differ or where the checks stopped. Export `SESSION.TXT` if you want to keep the result.
3. To repair an eligible cart, press **B** to return to modes and choose **Repair primary boot table**. If the checks pass, hold **L + R** and press **A**. The driver rechecks the source table and the target's shared spare snapshot, replaces only the 16 KiB primary table at `0x8000` and its ECC, and verifies the entire rewritten erase block. Restart to test uDisk boot.

This path uses the target's own NAND data and bypasses the safe-mode ROM. It applies to the observed 16 Gbit spare layout, with page 0's spare record blank, the three ARM7 records intact, and the repeated boot data present. It can use a supported uDisk or XuluMenu image that survives on the cart. It cannot restore an erased shared block or reinstall missing uDisk. The operation erases and rewrites the first physical erase block, with other main and spare bytes staged in RAM and preserved. Use a reliable power supply. The same-cart write has passed emulation and still needs a physical-cart test.

The preflight uses the table layout observed in Apache Thunder's updater. Its ARM9 code expands each 128 KiB NAND allocation into eight little-endian physical addresses spaced `0x4000` bytes apart. The tool checks the entries needed by the mapped image and checks the compact header and the first sectors of both ARM binaries. [XuluMenu updater reference](https://github.com/ApacheThunder/XuluMenu/tree/7ceabcbc9331f84a8c4fcde38b6f60193405ee8a/XulumenuUpdator)

## Windows dump tools

On Windows, install Python 3 with Tcl/Tk, then double-click **Start Recovery Toolkit.cmd** in this folder. You can also run `python3 recovery_tool.py` on a desktop with Tkinter. Keep the scripts and the `compiled` folder together for access to the DS builds and the banner template.

1. Use **Open DS tools folder** to find the combined `.nds` and [save both NAND views](#recovery-instructions).
2. In **Inspect and export**, select both dumps from the same cart. Inspect the boot regions, save a 512-byte copy of spare sector 0, or export the 16 KiB range at `0x40000` from the target main dump. Review the export against the [card's boot layout](#boot-layout-notes) before treating it as a replacement table.
3. For a banner repair, open **Banner repair**, select the compatible donor's main and spare dumps, and choose where to create `banner-ready`. Use the resulting `.nds` and payload with the [banner-only instructions](compiled/write-enabled/banner-only/).

The desktop tool works on dump files. The DS app handles NAND access and [same-cart repair](#repair-using-only-the-target-cart).

The Windows app saves a timestamped file under **N-Card Recovery Logs** in your user folder. The **Session log** tab shows the current session and lets you save a copy. The log records selected file paths, inspection results, exports, banner preparation, cancellations, and errors. To keep command-line output and errors, append `> nrio-session.txt 2>&1` to the command.

## Physical addressing

USB sector `n` maps to NAND main-data byte offset `n * 512`. The driver queries the chip ID for capacity and erase geometry. It bypasses the original block-table discovery, FAT volume offset, and logical block remapping. In the stock driver, `nrio_block_table_address` locates the FTL's physical block map, and the separate `nrio_nand_address` guards block low-address programming. The recovery driver uses explicit physical row and column addresses, including row zero, and checks requests against the detected chip capacity.

The USB image contains all addressable main data, including boot and other normally hidden regions. The 64 spare bytes per page are handled internally during writes. The 512-byte USB sector interface exposes main data only.

## Spare-area dump

The spare-area USB build packs eight consecutive 64-byte spare records into each 512-byte USB sector. The first 64 bytes in the dump belong to NAND page 0. For page `n`, the record starts at offset `n * 64` in the spare dump and its corresponding main data starts at `n * 2048` in the main dump. A 2 GiB N-Card should expose **131,072 USB sectors (64 MiB)** in the spare-area build. The exposed records include ECC and other spare bytes as returned by the N-Card bridge. Their contents can be compared between a working card and the damaged card. For a programmer dump that interleaves 2,048 main bytes and 64 spare bytes per page, the spare record for page `n` starts at `n * 2112 + 2048`. Compare individual spare records at these offsets. See the [ARM7 boot code finding](#arm7-boot-code-in-the-spare-area) for an example of boot data in these records.

## Spare recovery writes

Both spare modes use the same eight-record layout. Every 512-byte USB write supplies eight consecutive 64-byte spare records. The driver reads the whole affected erase block, replaces only the requested spare records, rewrites the block, and compares the readback of all main and spare bytes with the staged data. It preserves the supplied ECC and marker bytes verbatim. Use spare data intended for the exact main-page contents on the target card. Copying spare bytes between cards can also copy block markers and other metadata.

Save main and spare dumps before writing. The same erase-block, bad-marker, and power-loss limitations described below apply. See the hardware results below for the first successful spare-area recovery write.

## Recovery writes

Take and retain a full read-only image before selecting a write mode. Use physical offsets and an image appropriate for the target N-Card. A USB write can erase and rewrite an entire NAND erase block, even for a single changed sector. The driver stages that block in RAM supplied by the USB app, retains untouched data and spare bytes, updates NRIO ECC for changed sectors, and checks the resulting data and spare bytes by reading the block back. It stops further writes after a failure until driver startup. A request spanning two erase blocks can commit the first block before encountering an error in the second. It rejects unfamiliar spare markers in the first two pages of a block. The path has no remapping or power-loss recovery. Boot-region ECC and any hardware write protection still need testing on a physical card.

## Main-area write with block zero protected

The protected mode reads the full main area and rejects every write touching the first physical erase block before any erase or program command. This protects the region below `0x2000` during an erase-block rewrite. The first writable byte is `0x20000` on 64-page blocks or `0x40000` on 128-page blocks. The banner at `0x2000` remains readable. Later blocks can still be erased and rewritten. Emulation covers writes below and at the banner, requests that cross the boundary, and the first allowed write on both geometries.

## USB integration and rebuild

The DS app calls the [recovery backend](source/recovery.h) directly to select a mode, query capacity, bind the erase-block workspace, retrieve write diagnostics, or repair a boot table. USB sector requests use the backend's sector functions. The standalone [DLDI read callback](source/interface.c) accepts only physical main-area sectors within the detected capacity. The [NAND transport](source/nand.c) provides the common card probe and main-area reads.

Build the standalone raw driver with `make DEVKITPRO=/path/to/r20-sdk`. It uses a 4 KiB DLDI slot. For the `.nds`, use BlocksDS and a clean checkout of [Apache Thunder's nrio-usb-disk fork at b9b781b](https://github.com/ApacheThunder/nrio-usb-disk/tree/b9b781bf5deec5096cbc517432878430ac0e58c6). Run the following from `n-card/raw-nand` with `usb_checkout` set to that checkout's absolute path.

```bash
git -C "$usb_checkout" apply --unidiff-zero "$PWD/usb/toolkit.patch"
mkdir -p "$usb_checkout/source/recovery_nand" "$usb_checkout/source/card"
cp source/{nand.c,recovery.c,restore.c,raw_nand.h,recovery.h,banner_plan.h} "$usb_checkout/source/recovery_nand/"
cp ../source/card/nand_ecc.h "$usb_checkout/source/card/"
cp usb/{boot-repair.c,boot-repair.h,session-log.c,session-log.h} "$usb_checkout/source/"
make -C "$usb_checkout"
```

The build produces `nrio-usb-disk.nds` and `build/nrio-usb-disk.elf`. The ROM is ready to launch as built. [Third-party notices](compiled/licenses/)

## Verification

`python3 check_transport.py` tests the standalone DLDI in Unicorn, including range checks and rejection of the former reserved-sector commands. With `pyelftools` installed, pass `--app-elf "$usb_checkout/build/nrio-usb-disk.elf"` to also test the directly linked recovery code. The recovery tests cover mode locking, both erase geometries, bad markers, injected failures, and same-cart repair. Same-cart tests verify that changed source or spare data prevents erase and that the primary table and its ECC are the only changed data after a successful repair. The current DS app still needs a physical hardware test. The hardware observations below were made with earlier builds.

## Boot layout notes

Apache Thunder identified a four-byte value at main ROM header offsets `0x08–0x0B`. In the raw NAND screenshot, the `DSGBA PA` header begins at `0x400`, so the same bytes appear at NAND offsets `0x408–0x40B`. The standard DS header places these bytes at the end of the title field. The `DSGB` game code follows at header offset `0x0C`. [GBATEK DS cartridge header](https://problemkaputt.de/gbatek-ds-cartridge-header.htm)

| Sample reported by Apache Thunder | Bytes on disk | Little-endian word |
| --- | --- | --- |
| Branded N-Card, reported on 8 and 16 Gbit carts | `00 00 83 10` | `0x10830000` |
| Pokémon Mystery Dungeon bootleg, capacity unconfirmed | `00 00 43 10` | `0x10430000` |

The values differ by `0x00C00000` (bits 22 and 23). Apache reports that the boot initialization uses this area. The meaning of those bits and their relationship to the NAND ID, geometry, or capacity remain unverified. The offline inspector reports the value for a matching header. Keep the original bytes when preparing same-cart recovery data.

Apache Thunder described two layouts used by different generations of cards.

| Layout | Block table at `0x8000` | Block table at `0x40000` |
| --- | --- | --- |
| Early single-game DSFlash2 | Points to the flashed game, described at `0x40000` | In the described single-game layout, this offset is the start of the game |
| Newer uDisk N-Card | Points to a safe-mode ROM | Points to the normal uDisk ROM, usually at `0x80000` |

Apache recalls buying an N-Card around 2008 for roughly $40–$50 or more.

In the newer layout, the safe-mode ROM can update uDisk from a PC through the Slot-2 USB writer before handing off to normal uDisk. With the writer absent, Apache Thunder reports that the safe-mode ROM follows the second block table to load uDisk. The offsets in the table describe the reported layouts and may vary by card.

To enter USB update mode on a DS or DS Lite, insert the USB writer into Slot-2 and connect it to the PC. Hold **L + R + A + B + D-pad Up** while launching the N-Card. Keep holding the buttons until **USB SAFE BLOCK** appears on the bottom screen. The PC updater can then run even if normal uDisk fails to boot. [Apache Thunder’s XuluMenu flashing instructions](https://github.com/ApacheThunder/XuluMenu#flashing-instructions)

### DS Fire Card safe-mode ARM7 experiment

Apache Thunder confirmed that the DS Fire Card booted on a DSi with his modified safe-mode ROM. Apache installed the modified ROM by force-writing after putting spare block 0 into the bricked state. This confirms DSi boot after that workaround. The targeted installer was later tested on the DS Linker. The Fire Card itself still needs a targeted install test.

Apache reports an ARM7 payload of `0x38` bytes in the safe-mode ROM and `0x40` bytes in the main ROM. The hypothesis is that the first-stage ROM leaves eight bytes of handoff data at the end of the ARM7 destination and that loading safe mode on a DSi overwrites them. The role of those bytes and the cause of the suspected overwrite are unverified.

Apache's approach stages the safe-mode ARM7 stub and payload in main RAM. The stub copies the payload to the original ARM7 destination before jumping to the original entry point. The standard DS header has separate ARM7 entry, load address, and size fields. [GBATEK DS cartridge header](https://problemkaputt.de/gbatek-ds-cartridge-header.htm)

Apache supplied the tested `safe.nds` for main NAND offset `0x20000`. It is `0x14E40` bytes long and, if mapped contiguously, occupies `0x20000–0x34E3F`. That range lies within erase block 0 on the tested cart, which reported 128 pages per erase block. The ROM header stages `0x200` bytes of ARM7 code and data at `0x02380000`. Disassembly and emulation of the supplied stub confirmed that it copies `0x38` bytes to `0x03800000`, leaves the next eight bytes unchanged, and jumps to the copied code. That code waits for `0x1918027E` at `0x027FFE60` and then loads its jump target from `0x027FFE34`. This identifies a handoff in main RAM. The suspected overwrite in ARM7 WRAM remains unverified.

This approach keeps the primary table pointing to safe mode so the stock uDisk path can remain in place. It does not require installing XuluMenu. The supplied USB logs for NAND ID `2514d3ec` report 1,024 main sectors written (512 KiB total) and one packed spare sector written, both sessions with zero write errors. The logs record transfer counts. Verifying which bytes changed and whether the original uDisk data was preserved requires before-and-after NAND dumps.

Apache's later `dsfireline.bin` backup covers main NAND from `0x00000` through `0x7FFFF`. Its primary table maps the original `safe.bak` byte for byte starting at `0x20000`, and its secondary table points to uDisk at `0x80000`, immediately beyond the backup. Comparing Apache's patched image with the original, and independently preparing a ROM with the toolkit from that same backup, identified the same two affected NAND pages, `0x40` (ROM header) and `0x68` (ARM7 slot). Both are in erase block 0. Both relocation stubs copied the original ARM7 bytes and preserved the following eight bytes in emulation.

The supplied `dsfireline_spare.bin` covers only the first eight spare records (`0x200` packed bytes). A full snapshot of block 0 for this 128-page geometry requires the first `0x2000` packed spare bytes in addition to the first `0x40000` main bytes. The toolkit successfully prepares a test ROM from the supplied files and reports the missing spare coverage. The targeted installer requires the full block snapshot. Its physical write path still needs a hardware test.

### DS Linker safe-mode comparison

Apache supplied the unmodified and modified DS Linker safe-mode ROMs. The target's main backup maps the unmodified ROM byte for byte from `0x20000`. Apache's patch and the toolkit's patch both change physical NAND pages `0x40` and `0x66` and preserve the eight bytes after the relocated ARM7 payload in emulation. The original DS Linker and Fire Line ARM7 payloads differ by one 32-bit word. The DS Linker waits for `0x1118027E` at `0x027FFE60`. The Fire Line waits for `0x1918027E` at the same address. Emulation confirmed that the DS Linker branches with its own word and keeps waiting with the Fire Line word. Prepare the patch from the target cart's own safe-mode ROM.

The complete DS Linker capture contains `0x2000` packed spare bytes. The last non-`FF` byte was at `0x17F`. Apache used that capture and the original main dump to test the targeted installer on the unmodified DS Linker. The exported session log reported NAND ID `2514d3ec` and **Safe-mode install verified**, confirming the backup check, block-0 rewrite, and full readback on hardware. Apache completed the install on the DS Lite without PC interaction during the write, then confirmed that the DS Linker booted normally on both DS Lite and DSi with the toolkit-generated patch and its existing uDisk installation. He reported that earlier recoveries needed a uDisk reinstall after he ran his custom updater to enter a bricked state. The USB updater shortcut remains to be retested.

### Identify the safe-mode patch on a cart

Launch `nrio-raw-toolkit.nds` on a DS or DS Lite, insert the target at the detection screen, and choose **Check safe mode**. The app follows the primary boot table at `0x8000`, validates the mapped ROM header and ARM7 payload, and reports a supported stock ROM, the toolkit's DSi ARM7 patch, Apache Thunder's DSi ARM7 patch, or an unknown layout. Read failures are reported separately. The ROM and ARM7 NAND offsets and recognized handoff word appear in the exportable session log.

This check is read-only. It recognizes the Fire Line and DS Linker payloads and the two relocation stubs from the tested layout. Other images can report an unknown layout. Patch recognition confirms which ARM7 code is present. Test normal boot on a DSi to confirm compatibility on that cart.

### Prepare stock safe mode in the recovery toolkit

1. Save **Main: read only** and **Spare: read only** dumps from the same cart.
2. Open **Stock safe mode** in the Windows toolkit. Select the two dumps and the erase-buffer page count from the DS session log.
3. Choose **Prepare safe-mode ROM** and save `safe-mode-dsi-test.nds`. The session log shows the affected NAND erase blocks and flags missing backup coverage.

The toolkit follows the primary table at `0x8000` to extract a supported compact safe-mode ROM from the main dump. It stages ARM7 at Apache's tested address, `0x02380000`, using the same `0x200`-byte load size. The generated stub copies the original `0x38` bytes back into ARM7 WRAM before jumping to the original entry. The output retains the original ROM length. It uses the target's own backup and needs no separate ROM download or XuluMenu installation. Preparation runs offline and produces a ROM for testing. Use the targeted installer workflow below to prepare a DS app that can write the patch back.

To prepare the same file from the command line, use the page count from the target's DS log.

```sh
python3 inspect_boot.py target-main.bin target-spare.bin --prepare-safe-mode safe-mode-dsi-test.nds --pages-per-block 128
```

### Targeted stock safe-mode install

This path supports the compact ROM layout found on the Fire Line and DS Linker and requires both changed sectors to map to erase block 0. The desktop-prepared workflow passed installation, full block-0 readback, and boot on DS Lite and DSi on the DS Linker. The on-console preparation workflow has passed emulation using the Fire Line and DS Linker samples and still needs a hardware test.

#### Prepare and install on the DS

Use the generic `nrio-raw-toolkit.nds` on a DS or DS Lite. Keep a fresh main and spare backup on another device before installing. At the detection screen, insert the target cart and choose **Install stock safe mode**. The app reads all of block 0 and prepares the DSi ARM7 patch in RAM. It checks the original header, ARM7 code, available padding, and mapping, then shows both sector addresses. Press **B** to cancel or hold **L + R** and press **A** to install. The app rereads the block and verifies it against that snapshot before erase, then verifies the full rewrite. The RAM snapshot is discarded when the mode ends. Use **Check safe mode** afterwards or boot the cart on a DSi.

#### Use a desktop-prepared installer

1. Keep a fresh full block-0 backup from the target. For 128-page blocks, capture main offsets `0x00000–0x3FFFF` and packed spare offsets `0x0000–0x1FFF`. For 64-page blocks, capture main offsets `0x00000–0x1FFFF` and packed spare offsets `0x0000–0x0FFF`. The main dump must also cover the mapped safe-mode ROM.
2. In **Stock safe mode**, choose **Prepare targeted installer**. Save the generated `nrio-stock-safe-mode-installer.nds` privately. It contains the cart's original block-0 data, including the spare records.
3. Launch the prepared installer from a separate homebrew launcher on a DS or DS Lite, then insert the target cart at the detection screen. Choose **Install stock safe mode**. Hold **L + R** and press **A** to confirm, or press **B** to cancel. The app checks the live block's main and spare data against the embedded snapshot before any erase. If the snapshot matches, it stages the two mapped ROM sectors and their ECC, rewrites block 0 once, and verifies every main and spare byte against the staged image.
4. Keep the console powered until the verification result appears. Export `SESSION.TXT`, then test normal DSi boot with the target cart. Verify stock uDisk and USB safe mode on DS or DS Lite. Keep the original backups for recovery if a power loss or NAND write failure interrupts the rewrite.

The app refuses a changed snapshot, mismatched erase geometry, or invalid safe-mode mapping. The general USB writer retains its spare preflight. For CLI preparation, use `python3 arm_safe_mode.py target-main.bin target-spare.bin --pages-per-block 128` with the page count from the target's log.

Use the [offline patcher](patch_safe_mode.py) with the original, extracted safe-mode `.nds` and a confirmed free staging address in main RAM. Replace `STAGING_ADDRESS` with that address in hexadecimal.

```sh
python3 patch_safe_mode.py safe-mode.nds safe-mode-arm7-test.nds --stage-address STAGING_ADDRESS
```

The patcher checks the header CRC and the original `0x38`-byte ARM7 range. It requires enough padding immediately after that payload, keeps the ROM length and its ARM7 ROM offset, and updates the ARM7 RAM address, entry, size, and header CRC in the new file. It checks for overlap with ARM9 in RAM. Determine the staging address from the actual boot-time memory layout, including buffers used by the first-stage loader. Test the new ROM with the primary table still pointing at safe mode, then check normal launch on DSi and USB safe mode on DS or DS Lite. The patcher works on files only. The repository's generated stub passed emulation, including the exact `0x38`-byte copy and preservation of the next eight bytes. A ROM generated by this patcher still needs a Fire Card hardware test.

Before choosing a write location, save **Main: read only** and **Spare: read only** dumps from the target. Run the following with the cart's extracted, unmodified safe-mode ROM and the erase-buffer page count from `SESSION.TXT`.

```sh
python3 inspect_boot.py target-main.bin target-spare.bin --safe-mode-rom safe-mode.nds --pages-per-block 128
```

Add `--patched-rom safe-mode-arm7-test.nds` to report only erase blocks containing changed pages. The check compares every mapped byte in the original ROM's declared size against the main dump and flags any missing main or spare erase-block coverage. On the DS Fire Card that reported 128 pages per block, writes at `0x20000` and `0x30000` both touch erase block 0. The next boundary, `0x40000`, can contain the secondary uDisk table. Use the mapping report to plan a targeted write and keep the general preflight enabled.

### Direct uDisk boot test on a working 16 Gbit N-Card

Apache Thunder successfully redirected the primary block table at `0x8000` on a working 16 Gbit N-Card to boot its uDisk section directly, as in the [second-card recovery](#second-card-recovery-and-custom-banner). Apache deliberately put the card into the bricked state, wrote the corrected NAND dump, restored the card's shared-block backup, then reflashed the uDisk section with the XuluMenu installer.

Apache reported slightly faster boot after the change and saw no SRAM errors when launching games with existing saves. Apache had seen those errors on existing saves after bricking a card in earlier tests. The original safe-mode ROM appeared noticeably larger than those on the other tested cards and also failed on DSi in Apache's setup. Boot time and ROM size were reported qualitatively.

## Recovery instructions

For an unbootable N-Card, start the console with the target cart removed and launch `nrio-raw-toolkit.nds` through a separate homebrew launcher. Insert the target after power-on, then press **A** at the toolkit's detection screen. This keeps cartridge detection and boot-table repair independent of the target cart's ability to boot. [Apache Thunder’s DS Lite test setup](#launching-the-tools-with-an-unbootable-card)

1. **Back up both NAND views.** With the N-Card in Slot-1 and the USB writer in Slot-2, launch the toolkit in **Main: read only** and save a full disk image on the PC. Return to modes and choose **Spare: read only** to save a separate image. Cancel any prompt to initialize or format either disk. Record the card model and reported sector counts.
2. **Check how far the cart boots.** Record whether the DS menu shows the banner, whether launching the cart reaches uDisk or a white screen, and whether the [USB Safe Block shortcut](#boot-layout-notes) reaches the updater on carts with that layout. A working banner can return before uDisk boots, as it did in the [ExpressCard recovery](#full-boot-restored-on-the-expresscard).
3. **Match the repair to the finding.** Use a card-specific uDisk updater and confirm its target block table before flashing. The ExpressCard result applies to the `0x8000` variant used on that card. For a confirmed spare-area repair with replacement data appropriate for the target, select **Spare: recovery write**. Its USB sector 0 covers eight spare records, so preserve any records outside the intended change in the 512-byte payload. For a banner-only change, follow the [banner instructions](compiled/write-enabled/banner-only/) for the page at main offset `0x2000`.
4. **Verify the result.** Read back the affected main and spare data with the read-only modes and compare them with the expected bytes. Check the banner in the DS menu and launch the cart to verify that uDisk boots. Record which checks pass and any USB or DS error text.

### Assess recovery using only the target card

Run the [read-only boot inspector](inspect_boot.py) on separate physical main and packed spare dumps from the target card. Choose **Inspect dumps** in the desktop toolkit or run `python3 inspect_boot.py target-main.bin target-spare.bin > recovery-check.txt`. The report compares the first 16 KiB and the following 48 KiB of both candidate regions, checks the compact uDisk header and map entries, and lists the state of each shared spare record. It also flags spare bytes that would trigger the general write preflight. The 64 KiB comparison uses the maximum table size Apache Thunder reported. Differences outside the image's active entries do not establish the full table extent. The report leaves both dumps untouched. [Apache Thunder’s block table description](https://gbatemp.net/threads/open-source-flashcart-project-info.665781/page-2)

```sh
python3 inspect_boot.py target-main.bin target-spare.bin
```

- **Primary table damaged, secondary table and uDisk intact.** Try the [same-cart repair](#repair-using-only-the-target-cart) on the supported layout. The [16 Gbit test](#direct-udisk-boot-test-on-a-working-16-gbit-n-card) verified the resulting boot path using a corrected image and a subsequent uDisk reflash. The table-only repair still needs a hardware test.
- **uDisk damaged.** A compatible installer can supply the uDisk section, as in the [DS Linker recovery](#third-card-recovery-and-spare-area-readback). The boot table still needs to point at the installed copy.
- **Early boot data damaged.** Keep surviving main and spare bytes from the target. An earlier backup from that same cart can supply missing data. The [ARM7 copies](#arm7-boot-code-in-the-spare-area) and [repeated Blowfish bytes](#blowfish-data-in-the-spare-area) are potential sources for partial damage. A complete reconstruction of erased boot records without a backup remains unverified. A blank page 0 spare record alone is normal on the observed working 16 Gbit card.

For the offline check, save at least the first `0x50000` main bytes and the first `0x200` packed spare bytes from the same cart. Include the mapped uDisk image in the main backup to check its header and ARM entry sectors. A backup ending at `0x80000` can compare both table regions and inspect the spare records. It may end before the mapped uDisk header, which the report will flag. On an 8 Gbit cart that the DS repair rejects, keep the pre-brick backups and share the offline report and, if available, the DS session log. The DS writer still requires the observed 16 Gbit spare layout.

## Hardware results

Apache Thunder performed the hardware tests below and provided the recovery observations and DS Linker sample.

### Launching the tools with an unbootable card

Apache Thunder used HaxxStation and DS Download Play to launch the NRIO `.nds` tools on a DS Lite during recovery. He powered on with the target N-Card removed and inserted it into Slot-1 after startup, before entering Download Play. This let the NRIO tools access the target during stages when the cart itself could not boot. The current toolkit prompts for cartridge detection on launch and can retry before opening the mode menu.

Apache also launched a separate copy of uDisk through Download Play to reach XMENU and run the XuluMenu installer. With the USB writer inserted, holding **SELECT while uDisk starts** skipped the USB/Menu selection screen and opened XMENU directly.

### Raw USB readback

HxD on Windows showed the USB disk with 4,194,304 sectors of 512 bytes each (2 GiB) and displayed data starting at physical sector 0. The screenshot also shows `DSGBA PA` at byte offset `0x400` (sector 2). This confirms USB enumeration and reads at the start of NAND on the tested card. Apache Thunder reported that the main-area recovery build successfully wrote the intended data to the damaged card. That main-area write alone did not restore Blowfish initialization or the banner. A full dump and reads at the end of the device remain to be verified.

HxD also showed the spare-area device with 131,072 sectors (64 MiB). At spare-dump offset `0x035C8E00`, the screenshot shows the NRIO page marker `55 AA 55 AA` and the ECC triplet `5A 59 A5` repeated at offsets `+4`, `+8`, and `+12`. This matches the driver's normal metadata and ECC layout in the filesystem area. A comparison against a programmer dump remains open.

### Spare-sector 0 recovery

Apache Thunder used the writable spare-area USB build to copy USB sector 0 from a working N-Card to the damaged card. In this view, sector 0 is the 512-byte group of spare records for physical NAND pages 0–7. The recovery driver rewrites the affected erase block while preserving the staged main data.

After the spare write, Apache Thunder reported that **Blowfish initialization and the banner worked again**. A subsequent launch still stopped at a white screen. Apache Thunder planned to inspect the main area for remaining differences. The test involved all eight spare records, so it does not identify the specific page or byte range responsible for the fix.

### Full boot restored on the ExpressCard

After the spare-sector 0 repair restored Blowfish initialization and the banner, the card still stopped at a white screen. Apache Thunder then ran a custom uDisk updater targeting the block table at `0x8000`. Their updater also has a variant for the table at `0x40000`. The `0x8000` update restored full boot on this ExpressCard.

Apache Thunder also reported an earlier “device not ready” error while attempting a separate banner change. The cause of that error and the result of the banner change remain unconfirmed. The `0x8000` result applies to the tested card and updater.

### Banner write at byte offset `0x2000`

The reported `Write error [16, 2048]` describes a 2,048-byte USB write starting at LBA 16, which maps to NAND page 4. Page 4 is in erase block 0. Before erasing that block, the driver reads the first two pages and expects the first byte of their spare records to be `0xFF` or `0x55`. Boot-region spare data from the successful repair can contain other values. Apache Thunder reproduced the banner-write error on hardware. The diagnostic build reported `spare preflight: page 0x0, detail 0xbb`. The first spare byte on page 0 was outside the accepted values, so the driver refused the write before issuing an erase or program command.

The updated write-enabled USB apps print a second error line. For example, `spare preflight: page 0x1, detail 0x0` identifies the rejected spare byte and confirms that the driver stopped before erase. Other messages identify a failed read, erase, program, or readback stage. The preflight check remains enabled in the general recovery build. The [banner-only build](compiled/write-enabled/banner-only/) can use the target card’s saved spare records to authorize a specific page-4 replacement.

### Second card recovery and custom banner

Apache Thunder confirmed that the second N-Card recovered fully. The DS menu photograph from testing shows its custom icon and `DSGBA NAND CARD` banner. The source of that banner remains unconfirmed.

For the main NAND repair, Apache started with a dump from a working N-Card and replaced the block table at `0x8000` with a copy of the table from `0x40000`. On this layout, the first table then points directly to normal uDisk, bypassing the original safe-mode ROM and its USB update screen. Apache made the edit because the safe-mode ROM failed during DSi boot on the tested setup. The exact failure mechanism remains unknown.

### Shared block and early NAND writes

Apache Thunder had to **deliberately put each card into a specific bricked state** before writing to the region at main NAND offset `0x0000`. The goal was to restore the block table at `0x8000`. Apache observed that the area they called the **shared block** was mostly `0xFF` when the write succeeded. Why that state allowed writes remains unknown.

Apache provided `shared_dslinker.bin` from a third, partially bricked DS Linker before attempting to reproduce that state. The file has 135 `0xFF` bytes out of 512, including the entire range `0x180`–`0x1FF`. It also contains three identical 64-byte records at file offsets `0x40`, `0x100`, and `0x140`. In the packed spare-area view, these positions correspond to identical spare records for NAND pages 1, 4, and 5 and all-`0xFF` records for pages 6 and 7 of USB sector 0.

SHA-256 of `shared_dslinker.bin` is `66cb7c14f47246135ea4c737831a03d5e91324f3b8d238029386d35df61cf853`.

For the follow-up test, Apache said they wrote a regular NAND image with its first byte (image offset `0x0000`) changed to lowercase ASCII `h` (`0x68`). Apache made that change because nrioTool had returned incorrect dumps without it. The reason this byte affected the dump remains unknown. Apache identified the `MKR` markers appearing every `0x200` bytes in the screenshot as artifacts of extracting the hex data from the custom uDisk updater. Exclude those markers when interpreting the screenshot as raw NAND data.

The bricked shared block then read entirely as `0xFF` in the spare-area view. HxD shows all bytes in spare-view USB sectors 0 and 1 (`0x000`–`0x3FF`) as `0xFF`, covering the spare records for physical NAND pages 0–15. The `0x68` edit was in the image Apache wrote. The `0xFF` bytes were observed in the separate spare-area readback. The full extent of the affected NAND region remains unconfirmed.

### Third card recovery and spare-area readback

Apache confirmed that the third card, the DS Linker, also recovered. They wrote the corrected main NAND contents up to `0x80000` with the modified DLDI, then restored the original shared-block backup. Apache reported having to rerun the XuluMenu installer after each recovery and suspected that the uDisk section needed reflashing. The reason the reinstall was needed remains unconfirmed. [How Apache reached the installer](#launching-the-tools-with-an-unbootable-card)

Between the main-area write and the shared-block restore, HxD showed non-`0xFF` bytes in the spare records for pages 0, 4, and 5. For example, page 0 shows `96 A9 56` at spare offsets `+4`, `+8`, and `+12`. This pattern matches the raw main-area driver's ECC layout. The driver regenerates those triplets for changed 512-byte sectors and carries the other staged spare bytes forward when it rewrites the erase block. Apache suspected that the uDisk updater clears more of the shared block. The updater's exact erase and rewrite behavior remains unverified. [Raw NAND write implementation](source/restore.c)

For a paired capture, use **Spare: read only** from the [DS mode menu](#ds-mode-menu) to save the same range at each stage. Compare the images in 64-byte records and record which page records changed, which updater and target offset were used, and whether the banner and uDisk boot worked at each stage. Intentionally clearing the shared block can leave the card unbootable.

### ARM7 boot code in the spare area

On the working 16 Gbit N-Card before the block-table edit, page 0's spare record (`0x00`–`0x3F`) reads entirely as `0xFF`. Apache Thunder identified the next 64-byte record (`0x40`–`0x7F`, NAND page 1 spare) as the initial cartridge ROM's ARM7 code. The selected bytes in HxD match the saved DS Linker sample byte for byte. Both show copies at spare-dump offsets `0x100` and `0x140` (pages 4 and 5).

The start of the record disassembles as little-endian ARM code. It loads `0x04000208` and writes zero there and eight bytes later, then polls a memory word and branches through an address read from memory. Which copy the cartridge uses at boot remains unverified.

### Blowfish data in the spare area

Apache Thunder identified the highlighted bytes at `0xC0`–`0xFF` in `shared_dslinker.bin` as part of the missing Blowfish data. That 64-byte range is NAND page 3's spare record. In the saved dump, the final 56 bytes of that record (`0xC8`–`0xFF`) match the first 56 bytes of page 2's spare record (`0x80`–`0xB7`) byte for byte. The data therefore repeats across the page boundary. This places the candidate Blowfish data in the same spare-view USB sector whose restoration brought back Blowfish initialization on the ExpressCard. The exact boundaries of the Blowfish material and how the cartridge loads it remain unverified.

Apache's screenshot of the 8 Gbit programmer dump (`nrio_8gbit_rawdump.bin`) shows the 64 bytes at `0x1880`–`0x18BF` matching page 2's spare record at `0x80`–`0xBF` in `shared_dslinker.bin`. The match is consistent with 2,048 main bytes followed by 64 spare bytes per page. In that layout the page 2 spare record starts at `2 * 0x840 + 0x800 = 0x1880`. Page 3's spare record would start at `0x20C0`, outside the screenshot.

The ASCII sequence `RMR` (`52 4D 52`) appears at `0x89` and `0xD1` in the spare-only dump, at the same position within the repeated bytes. The first occurrence is visible at `0x1889` in the programmer screenshot. Its meaning remains unverified.

### Banner-only write verified on hardware

Apache Thunder used the banner-only build on a working N-Card and confirmed that the DS displayed the new banner and the card continued to boot fully. The first banner copy changed. The second copy's role remains unverified. [Banner-only build and hardware notes](compiled/write-enabled/banner-only/#hardware-result)
