# uDisk 1.45 reconstruction

The stock BIOS updater installs uDisk in hidden NAND. The installed uDisk runtime offers USB storage, FAT formatting, and booting an existing file from the cart. The C in this directory reconstructs the paths listed below. It builds independently and uses host callbacks for I/O so the programming and launch logic can be exercised without a cart.

## Reference binaries

The recovered `update.nds` has SHA-256 `3391af866e42057576c2c9841cf748242612fde77bd3853fceaecaaea482ca11`. Its ARM9 starts at file offset `0x200` and loads at `0x02000000`. The BIOS updater's NitroFS entry `udisk_eng.nds` occupies file offsets `0x24200–0xE53FF`. It has 1,536 records of 515 bytes each. Each record carries 512 bytes of runtime data and a three-byte ECC tag. Removing the tags results in a byte-identical copy of the archived 768 KiB `udisk.nds`. [Recovered N-Card firmware](https://github.com/m4x10187/fire-card-archive/tree/ced86434054e47dcca8d4e90efd25cc92f88d688/N-Card/Dumps) · [uDisk 1.45 update package](https://linfoxdomain.com/nintendo/ds/dl/NCard_v1.45_v2.53_v25.zip)

The installed dump starts its ARM9 at file offset `0x200`, loaded at `0x02000000`. Its ARM7 starts at file offset `0x8BA00`. The startup code copies ARM7 from `0x02380000` into WRAM at `0x037F8000`. Use the WRAM address when tracing absolute references in its body.

## BIOS updater

| ARM9 address | C source | Behavior |
| --- | --- | --- |
| `0x0200042C`, `0x020011A0` | [updater.c](updater.c) | Checks the physical NAND address and encodes the erase frames |
| `0x020033B8`, `0x0200362C` | [payload.c](payload.c) | Computes the ECC tag and distinguishes a clean sector, a correctable data bit, a damaged tag, and an uncorrectable mismatch |
| `0x02000460` | [payload.c](payload.c) | Unpacks and checks the embedded runtime into caller-owned memory |
| `0x02000614`, `0x020006DC` | [updater.c](updater.c) | Walks 128 KiB allocations, programs through callbacks, retries, and writes the secondary map |
| `0x020007B8–0x020007E8`, `0x020003D8` | [updater.c](updater.c) | Expands the allocation list into a 64 KiB map and computes the stock readback byte sum |

The updater begins at physical main NAND `0x80000`. It attempts each 128 KiB allocation up to seven times and moves on if the byte sums keep disagreeing. A 256 KiB erase block is erased when programming its first half. After recording the verified allocations, the updater builds the secondary map at `0x40000`. Its programmed 128 KiB chunk includes the 64 KiB map, erased padding, and the footer `03 00 00 00 67 61 6F 00`. The original low-level erase and program entry points skip commands below `0x40000`.

The unpacker checks the archive's final erased-sector tag pattern and fails immediately on an earlier invalid tag. The stock loop can continue after an error and let that final record overwrite its status. The C update flow returns errors to the caller where the BIOS updater displays a message and stops in a loop. It expects the decoded payload padded to whole allocations.

The ECC packing shares the existing NRIO parity routine. This confirms the driver and updater use the same ECC calculation for ordinary 512-byte data. The updater's file tags and the cartridge's early boot spare records have distinct roles. The tagged payload contains the installed runtime and supplies no factory block-0 image. [Block-0 recovery findings](../raw-nand/README.md#fully-erased-block-0-without-a-backup)

## Installed runtime

| Address | C source | Behavior |
| --- | --- | --- |
| ARM9 `0x0200388C` | [runtime.c](runtime.c) | Checks the 31-byte USB Bulk-Only Transport wrapper and selects one of ten supported SCSI commands |
| ARM9 `0x020037C4` | [runtime.c](runtime.c) | Decodes the byte position and transfer count used by the command paths |
| ARM9 `0x02000E54` | [runtime.c](runtime.c) | Formats a FAT12 or FAT16 `DSGBAFLS` logical volume |
| ARM9 `0x020008E4` | [runtime.c](runtime.c) | Looks for launch files in priority order and expands their FAT cluster chain into physical 16 KiB addresses |

The ARM9 dispatcher accepts a single logical unit. It passes READ(10) and WRITE(10) to handlers at `0x02005B8C` and `0x02005A94`. They check that the CBW byte count matches the CDB sector count and call the runtime's NAND filesystem I/O. The formatter writes two FAT copies in 64 KiB chunks, clears the root directory, and writes the `DSGBAFLS` boot sector last. `udisk_format_volume` reconstructs that write sequence through a logical-sector callback and stops on a write error. The stock UI calls the NAND setup dispatcher at ARM9 `0x0200AEC0` in format mode before it runs this function. That dispatcher initializes the logical-to-physical map and capacity. A cart without that map needs this preceding step before it can accept the formatter’s logical writes. The standalone DLDI looks for the existing physical block table using spare marker `0xAB`.

The launch path looks for `/autorun1.nds`, `/autorun2.nds`, `/autorun3.nds`, then `/xmenu.dat`. It maps each cluster in 16 KiB steps, tries to install the map at NAND addresses from `0x200000` to just below `0x300000` in 128 KiB steps, and sends path-dependent `C1` cartridge commands before reset. The C reports the selected path and map, and checks the caller's output capacity before filling it.

The ARM7 entry in WRAM at `0x037F8C7C` coordinates the initial ARM9 handshake via IPC sync. In the writer-present branch, it checks a Slot-2 register for `0x1581` and uses shared state at `0x027FF038`. The USB branch waits for ARM9 to set state 2, while the launch branch at `0x037F8B84` waits for ARM9 to write `0xEF000000` at `0x027FFE04` and the address of that word at `0x027FFE24`. It then prepares code at `0x023FEE00` and signals the next stage. This gives us specific addresses to probe for the HBMenu launch hang. We still have no trace of that failure.

### NAND format metadata

The stock format setup keeps a four-KiB bad-block bitmap in the one-MiB window starting two MiB before the FTL data base. The first two 2 KiB pages use spare marker `0xAA` at byte 11. The next one-MiB window holds the physical block table with spare marker `0xAB`. The stock writer checks candidate blocks in erase-block order and verifies the pages after programming.

The native **Full Format** tests the filesystem's data blocks with a row-dependent program/readback pattern between two verified erases. It skips blocks from the existing bad-block records and persists newly failed blocks in the stock `0xAA` bitmap before publishing the replacement `0xAB` physical table. Quick Format does not run the write test or change the bitmap. Both leave the early boot area and hidden firmware untouched. Full Format is a native implementation, not a byte-for-byte reconstruction of the stock full-format routine.

`udisk_ftl_metadata_ranges` describes those windows. `udisk_encode_physical_table` serializes the full 64 KiB table from physical block assignments. Each two-byte entry is the upper half of the physical byte address in big-endian order. The encoder preserves the stock identity-filled tail and its 32-bit address wrap. `udisk_prepare_physical_table` takes a supplied bad-block bitmap, reassigns marked slots from the high end of the data area, stores the capacity entry, and calculates the usable logical capacity. It excludes marked blocks even at the end of the range, where stock can leave one counted. `udisk_write_ftl_metadata` walks the stock metadata windows and publishes the bitmap before the physical table. Its callback must erase, program, and verify each block. It can retry only if no visible metadata marker remains on the failed candidate. The native Quick Format uses this table encoding but leaves the existing bad-block bitmap in place, as the stock Quick Format does.

## DSi hardware finding

On a 16 Gbit N-Card (NAND ID `2555D5EC`, 128 pages per erase block), the installed uDisk 1.23 booted from the DSi System Menu after patching safe mode, but GodMode9Nrio hung while mounting the cart. Launching the stock 1.45 `nand_stable_25/udisk.nds` from the DSi SD card through Unlaunch opened the format menu immediately, without holding **START + SELECT**. Selecting **QUICK FORMAT** fixed the mount hang on the next try. No full format was needed. [Stock uDisk 1.45 package](https://linfoxdomain.com/nintendo/ds/dl/NCard_v1.45_v2.53_v25.zip) · [Apache Thunder’s Unlaunch notes](https://github.com/ApacheThunder/XuluMenu/releases)

**Recommended for an older uDisk filesystem that hangs during mounting:** Back up any files you want to keep, then use **QUICK FORMAT** from the stock uDisk 1.45 package. This verifies that the stock 1.45 formatter made the cart mountable on this setup. It does not verify that the installed uDisk boot ROM was upgraded. Quick Format wipes the cart’s filesystem. Block-0 backups do not include games or saves.

### Installed 1.23 to 1.45 migration

After the format, the N-Card file manager could read `DSYSTEM` and `xmenu.dat`, but the installed 1.23 still failed on direct boot. The same files worked when the stock 1.45 runtime was launched from Unlaunch. Installing the stock 1.45 BIOS updater via `fat:/autorun1.nds`, removing that file afterward, and booting from the DSi System Menu fixed direct boot on the test cart. No second format was needed.

The old dump has an outer ARM9/ARM7 boot wrapper. It stages the embedded SRL at logical offset `0x8000`. The primary safe-mode ARM9 selects the secondary table at physical `0x40000` before handoff. The installed 1.23 inner ARM9 has two independent startup failure paths.

- At `0x02005590`, the startup free-space wrapper checks for at least 1 MiB. The wrapper returns zero when a filesystem query fails, so the resulting low-space message does not establish that the NAND is full.
- At `0x02005764`, the startup routine looks up `/dsystem` and expects a directory result. It displays the missing-folder error if the lookup fails, even when that directory is visible to a different FAT driver.

The tested 1.23 layout has a secondary map footer `02 00 00 00 67 61 6F 00`. The stock 1.45 updater replaces the firmware payload and secondary map, including the footer revision, without updating the early safe-mode ROM. The [nrioTool setup](../nrioTool/README.md#udisk-setup-on-dsi-and-ds) recognizes the tested legacy wrapper, inner header, and complete secondary map before preparing any writes. The native installer replaces that handoff. It uses the stock 1.45 runtime as an embedded data image only. The original BIOS writer sends four main sectors via the card bridge and does not send a spare-area payload. The native writer matches that spare layout, protects unfamiliar spare markers, checks each erase block before writing, saves backups when independent storage is available, verifies each full readback, and replaces the secondary map last. Neither the manual success nor this fingerprint is evidence that every 1.23 variant has the same layout.

### Quick Format setup trace

For NAND ID `2555D5EC`, the stock dispatcher at ARM9 `0x0200AEC0` selects 256 KiB erase blocks, 8,128 data blocks, and a data base of `0x01000000`. Quick Format uses mode 1. It loads the existing bad-block bitmap (or starts with an empty bitmap if it can't read one), then erases and checks data blocks in 64-block zones. It skips blocks already marked bad. Full Format, mode 2, also runs the block test and publishes the refreshed bad-block bitmap first. Both paths rebuild the physical map before the FAT writer runs. Quick Format is more than a FAT header rewrite.

Run `python3 test_quick_format.py /path/to/udisk.nds` with Unicorn installed to reproduce the mode 1 and mode 2 traces up to the first erase or full block test. The trace substitutes NAND reads and does not write to any device.

### Native Quick Format

[format-native.c](format-native.c) performs the erase scan, bad-block exclusion, FAT staging and map publication using physical NAND callbacks. It emits the stock FAT bytes and marks filesystem pages using the existing NRIO spare and ECC format. The native path verifies the FAT pages before publishing the physical block map, so it does not depend on a mounted old filesystem or running the stock formatter. It does not claim byte-identical physical placement of FAT blocks. The on-console entry point is limited to the inspected 16 Gbit geometry.

```sh
cc -std=c11 -O2 -shared -fPIC format-native.c runtime.c payload.c -o /tmp/udisk-native.so
python3 test_native_format.py /tmp/udisk-native.so
```

The native format test compares the data read through simulated persistent NRIO markers with the stock formatter's sector output. It also checks that known bad blocks are preserved, the boot region is never erased, failures don't publish behind a stale map, and the map is published only after FAT verification.

## Build and verification

Build the standalone C library on a host with a C11 compiler, or set `CC` and `AR` to the devkitARM tools and add the ARM9 flags. Use separate build directories for the two targets.

```sh
make
make BUILD=build/arm9 CC=arm-none-eabi-gcc AR=arm-none-eabi-ar \
  CFLAGS="-O2 -g -mcpu=arm946e-s -marm -ffreestanding"
```

The library was checked against the archived ARM9 in emulation. Address limits, erase frames, full map bytes, and byte sums matched. The tagged payload unpacked to the archived runtime, including when a single data bit was changed for testing. The update trace matched for both erase sizes and for skipped allocations, map retries, and map failure. Runtime checks covered all 256 dispatch opcodes, malformed wrappers, transfer ranges, eight byte-exact formatter write traces and FAT boot sectors, and the launch map over a test FAT chain.

The portable build has no physical NAND, USB controller, or reset bindings. ARM7 drivers, complete runtime FTL, updater UI, and end-to-end boot or USB behavior are still open. Neither original firmware binary is included.
