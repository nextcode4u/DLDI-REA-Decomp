# Block-0 and boot replay results

These are offline comparisons and emulation tests, not results from flashing
a cartridge. No writer or hardware support was added.

## Held-out block-0 reconstruction

The inputs were a DSFlash2-prepared first stage from a separate ExpressCard
logical dump, the separately supplied original safe-mode SRL for each target,
and a previously identified contiguous placement and ARM7 handoff. The scoring
tool constructs its byte claims before opening either target dump. This tests
known component mappings, not automatic identification on an unknown cart.

| Target | Exact main bytes / claimed | Exact spare bytes / claimed | Important gaps |
| --- | --- | --- | --- |
| Fire Line | 91,736 / 91,736 | 320 / 320 | Filling unclaimed main bytes with FF would miss 7,451 programmed bytes. Only 512 of 8,192 spare bytes were captured. |
| Earlier DS Linker | 87,620 / 87,636 | 320 / 320 | The independent later first stage differs in 16 encrypted bytes. Filling unclaimed bytes with FF would miss 50,145 programmed main bytes and 64 spare bytes. |

The safe-mode ROM and its used primary table entries match on both. So do
spare pages 1–5, using the supplied handoff variant for the three copies of
ARM7 and the DSFlash2 table for pages 2 and 3. Spare page 0 is deliberately
unclaimed. It is all FF in the captured Fire Line sample, but all 64 bytes
differ from FF in the earlier DS Linker sample. An erased cart still cannot
tell us which page 0 or missing main bytes it needs.

See [the original writer analysis](DSFLASH2.md) for the independent first-stage
preparation and physical/logical offset relationship. The offline scorer
does not write an output image.

## Earlier DS Linker handoff

The ExpressCard first-stage ARM9 sender at `0x020005C8` writes
`0x1918027E` to RAM mailbox `0x027FFE60`. The actual ARM7 wait loops proceed
with their own value and stay in the loop with the other revision's value.

A separate follow-up used the 16 ciphertext mismatches to test a hypothesis
about the earlier cart. Changing the later logical image's two ARM9 literals
at file offsets `0x4588` and `0x45E4` from `0xB918027E`/`0x1918027E` to
`0xB118027E`/`0x1118027E`, then running the original DSFlash2 preparation
routine in x86 emulation, reproduces **all 2,048 bytes** of the earlier
capture's encrypted ARM9 window, including the 16 mismatches. Its 4 KiB
table also matches. The reproduced sender writes `0x1118027E`. In replay,
that releases the earlier ARM7 waiter and stalls the newer one.

This is strong evidence for why that earlier first stage and a newer
safe-mode ROM cannot hand off normally. It is not a full console boot
test or proof that every FPGA board has the same first stage. Prefer
patching the target cart's own safe-mode ROM. Do not replace the whole
first stage on the basis of this replay.

## Fire Link 8 Gbit secondary map

The earlier and newer DS Linker selectors and the Fire Link selector were
replayed with the captured Fire Link data. The original ARM9 code selects
the secondary table at `0x40000` in the tested scan branch, skipping to
`0x60000` if the first pre-table marker is changed to `QAFE`. Their tested
normal path sends the same `C1 10BF` selector. The emulator intercepted card
I/O at function boundaries. It did not model which extra entries the ASIC
might read during a real boot.

- Following the map in the host model requires **37 of its 512 entries** to
  reconstruct the declared compact uDisk 1.45 ROM. The stock updater's
  normal write uses six 128 KiB allocations, or 48 entries.
- Of the 464 entries past that write window, the dump covers 23 complete
  chunks. Seven contain non-FF data. At least 22,475 bytes in three spans
  match the archived earlier uDisk at the same relative source offset.
  That is consistent with residue from an earlier firmware, not proof of
  what the bridge uses or that the remaining entries are disposable.
- With both erase sizes, the original uDisk 1.45 ARM9 updater generates and
  publishes a new map that replaces all 464 extra entries. It does not
  program their payload addresses on the tested success path. There is
  no carry-forward of those old mapping entries.

This explains what the updater would discard and why blindly applying
the normal six-allocation layout is a bad compatibility change. The Fire
Link 8 Gbit layout stays read-only. The additional tail and live ASIC
accesses are not known. [Original Fire Card captures](https://github.com/m4x10187/fire-card-archive)

## Reproduce with your captures

The ROMs and updater are not redistributed. The audit command requires a
physical main dump and its packed spare dump.

```sh
python3 audit_reconstruction.py main.bin spare.bin --pages 128 \
  --prepared-stage1 prepared-stage1.bin --logical-stage1 logical-stage1.nds \
  --safe-rom stock-safe.nds --handoff 0x1118027e
```

With Unicorn installed, `replay_boot_gaps.py --help` lists the ROM and dump
inputs used by the replay. The optional earlier-handoff check takes the
original DSFlash2 0.3Z executable, the later logical first stage with its
original secure-area marker, and the earlier target's physical main dump.
The emulator runs only the identified preparation routine, never the USB
or Windows program.

The remaining hardware-dependent gaps are listed in [RECOVERY-GAPS.md](RECOVERY-GAPS.md).
