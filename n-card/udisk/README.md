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
| ARM9 `0x02000E54` | [runtime.c](runtime.c) | Builds the FAT12 or FAT16 `DSGBAFLS` volume boot sector |
| ARM9 `0x020008E4` | [runtime.c](runtime.c) | Looks for launch files in priority order and expands their FAT cluster chain into physical 16 KiB addresses |

The ARM9 dispatcher accepts a single logical unit. It passes READ(10) and WRITE(10) to handlers at `0x02005B8C` and `0x02005A94`. They check that the CBW byte count matches the CDB sector count and call the runtime's NAND filesystem I/O. The formatter writes two FAT copies, a cleared root area, and the generated boot sector. The C formatter reconstructs that sector only.

The launch path looks for `/autorun1.nds`, `/autorun2.nds`, `/autorun3.nds`, then `/xmenu.dat`. It maps each cluster in 16 KiB steps, tries to install the map at NAND addresses from `0x200000` to just below `0x300000` in 128 KiB steps, and sends path-dependent `C1` cartridge commands before reset. The C reports the selected path and map, and checks the caller's output capacity before filling it.

The ARM7 entry in WRAM at `0x037F8C7C` coordinates the initial ARM9 handshake via IPC sync. In the writer-present branch, it checks a Slot-2 register for `0x1581` and uses shared state at `0x027FF038`. The USB branch waits for ARM9 to set state 2, while the launch branch at `0x037F8B84` waits for ARM9 to write `0xEF000000` at `0x027FFE04` and the address of that word at `0x027FFE24`. It then prepares code at `0x023FEE00` and signals the next stage. This gives us specific addresses to probe for the HBMenu launch hang. We still have no trace of that failure.

## Build and verification

Build the standalone C library on a host with a C11 compiler, or set `CC` and `AR` to the devkitARM tools and add the ARM9 flags. Use separate build directories for the two targets.

```sh
make
make BUILD=build/arm9 CC=arm-none-eabi-gcc AR=arm-none-eabi-ar \
  CFLAGS="-O2 -g -mcpu=arm946e-s -marm -ffreestanding"
```

The library was checked against the archived ARM9 in emulation. Address limits, erase frames, full map bytes, and byte sums matched. The tagged payload unpacked to the archived runtime, including when a single data bit was changed for testing. The update trace matched for both erase sizes and for skipped allocations, map retries, and map failure. Runtime checks covered all 256 dispatch opcodes, malformed wrappers, transfer ranges, eight byte-exact FAT boot sectors, and the launch map over a test FAT chain.

The portable build has no physical NAND, USB controller, or reset bindings. ARM7 drivers, complete runtime FTL, updater UI, and end-to-end boot or USB behavior are still open. Neither original firmware binary is included.
