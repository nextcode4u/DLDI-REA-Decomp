# N-Card recovery gaps and hardware checks

## Fully erased block 0

These comparisons use physical offsets and standard CRC32, not the inverted
CRC convention in the `backup.info` file format.

| Read-only window | Fire Line | 8 Gbit DS Linker |
| --- | --- | --- |
| First sector at `0x40000` | `7DE2DB04` | `7DE2DB04` |
| Full 128 KiB at `0x40000` | `53CEF9E1` | `D23E09AF` |
| First sector at `0x80000` | Not captured | Not captured |

The first 64 KiB of both tables match byte for byte. The full windows differ
only in trailing padding (`0x00` vs `0xFF`) and the `gao` footer revision.
The block-0 boot headers and spare records differ. Replacing firmware can
change the surviving map anyway, so the difference does not identify which
block 0 is correct.

A verified known-good reference can recreate every byte of the corresponding
block in simulation. It does not prove another cart with that NAND ID uses the
same early ROM configuration, boot key material, ARM7 handoff, or spare page
0. In the current evidence the spare page 0 contents are not derivable from
the active map. No automatic block-0 generator or cross-cart writer was enabled.

See [the held-out reconstruction and emulator replay](RECOVERY-REPLAY.md) for exact byte counts and map behavior.

## FPGA safe-mode handoff

The stock ARM7 stubs from the two supplied DS Linker revisions differ in
exactly one byte at ARM7 offset `0x37`. One waits for `0x1118027E` and the
other waits for `0x1918027E` at main RAM address `0x027FFE60`. Both read the
next jump target from `0x027FFE34`. The ARM9 programs also differ. The later sender and a byte-matched
reconstruction of the earlier sender reproduce the mailbox mismatch in
emulation. An FPGA cart boot test is still needed to confirm behavior.
The replacement confirmation now warns about a changed ARM7 handoff or
ARM9 program without blocking an otherwise valid custom replacement.

## Most useful remaining tests

| Gap | Read-only evidence or hardware test that would move it forward |
| --- | --- |
| Wiped block 0 | Save `CHECK-CART.TXT` and a matched block-0 main/spare backup from two working carts of the same confirmed board revision. A raw main capture through `0x801FF` records the normal uDisk anchor too. Compare these with the bricked cart before trying a reference restore. |
| Early FPGA DS Linker | A `CHECK-CART.TXT` from the working cart and its own safe-mode SRL. Compare the ARM9 CRC and uDisk route against the failing newer ROM. Showing the safe-mode screen does not test the uDisk handoff. |
| Fire Link 16 Gbit | Save a block-0 backup, try the existing bypass-only operation on a matching board, then restart. Emulation verified the write and readback, not real-cart boot. |
| Fire Link 8 Gbit and 1.23o | Collect the full secondary map and a physical main/spare capture covering the active allocations. Current dump headers do not prove which extra entries may be discarded. The 8 Gbit captured map reaches `0x87FFFF`. |
| Ultra N-Card 1.23o | Get a surviving copy of the original firmware package or a read-only dump from a confirmed cart. The [original listing](https://gbatemp.net/download/ultra-n-card-card-update.458/) is not a working firmware download. |
| NinjaPass Jr | Get the board and chip markings and a raw capture. A [contemporary owner reported](https://gbatemp.net/threads/ninjapass-saving-guide.36147/) using the ndsgba.net client on the Junior, but that does not establish physical NAND offsets or the same boot map as a later N-Card. |
| YJLoader | Keep it separate pending a raw capture or bus trace. The [documented board](https://gbatemp.net/threads/mystery-flashcart-used-by-dora-bootleg.656146/) has a distinct `YJYJ` loader and SPI save chip. |
| 4 GiB NAND | Get the exact chip marking, use the chip's real page size, and send the `NAND-4GIB.TXT` log. A nonblank, repeatable upper-half sample from more than one address can support distinct reads, but cannot establish write or filesystem compatibility. |

Existing recovery and write guards are unchanged.
