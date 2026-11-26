# DSFlash2 image and boot-spare findings

The early DSFlash2 PC GUI and the later uDisk console writer are different programs. The uDisk suite and the XuluMenu USB package contain identical stock copies of `USBNDS.exe`. The modified `USBNDS_Bootleg.exe` changes the payload start from `0x80000` to `0x40000` in the allocation and write setup. That does not establish a placement rule in the older GUI or on a NinjaPass. [Archived N-Card suite](https://linfoxdomain.com/nintendo/ds/dl/NCard_v1.45_v2.53_v25.zip) · [Apache Thunder's USB writer builds](https://github.com/ApacheThunder/XuluMenu/tree/7ceabcbc9331f84a8c4fcde38b6f60193405ee8a/USB)

## What the archived client does

In the unpacked `dsflash2.exe` included in the 0.3Z archive (SHA-256 `24ee34db7a3944d3eaf3880c90732098dd28c02c7d7505d596d3404eaab7d38a`), the traced Burn path reads the first list item, rounds the buffer to 128 KiB, calls image preparation at x86 VA `0x406880`, and starts its writer at logical offset 0 in 128 KiB steps. That offset is in the host-to-writer address space. It doesn't tell us where the writer placed a separate menu in raw NAND. No multi-ROM image generator was verified in this path. The archive's 0.2Z client is packed and was not traced. [Original DSFlash2 utilities](https://github.com/m4x10187/fire-card-archive/tree/ced86434054e47dcca8d4e90efd25cc92f88d688/Ultra%20FlashPass%20EX/Utilities)

For an offline check, we ran only the 0.3Z image-preparation routine in x86 emulation on the archive's `DSpass.nds`, supplying a thread-local scratch area for the original PRNG and skipping its status message. No Windows application, USB writer, or physical cart was run. The input is a standard DS image whose secure area starts with the decrypted/dump sentinel. The generated image has populated tables at `0x1000` and `0x2000`, and its calculated secure-area CRC matches its updated header (`0xD87D`). The input's direct CRC was `0xCA53`, versus its stored `0x4296`. This demonstrates why a logical dump and a prepared writer image can disagree on secure-area CRC without one being a corrupt capture.

## DS Linker 1.22G wrapper

linFox's `DSLinker_Firmware_122G.rar` contains two 827,968-byte stock writer images, `udisk.nds` and `NAND_8G_256K.nds`. Both have a valid `DSGBA PASS` outer header and a second valid DS header at file offset `0x8000`, with a declared inner size of `0xC2240`. The outer ARM7 code reads from the card at logical offset `0x8000` at `0x02380230` and adds the same base to inner section offsets before reading them. This confirms an actual nested loader rather than a coincidental byte pattern. The bundled `USBNDS.exe` is byte-identical to the one in the later N-Card 1.45 package. The offset is relative to this writer image, and does not identify the NinjaPass NAND layout. [DS Linker 1.22G archive](https://linfoxdomain.com/nintendo/ds/dl/DSLinker_Firmware_122G.rar)

The separate `DSLinker_flashformattools.rar` contains the Windows GUI `flashformat.exe` and `EnumDll.dll`. Its format prompt names a disk and the binary contains the `DSGBAFLS` volume ID. We inspected it without running it, and haven't established the format algorithm from this executable. [DS Linker format tool archive](https://linfoxdomain.com/nintendo/ds/dl/DSLinker_flashformattools.rar)

The boot report now fingerprints the archived 1.22G wrapper and four compact N-Card runtimes, uDisk 1.33 beta, 1.41, 1.43 and 1.44. The two sectors for the 1.22G match are the outer and nested headers. The compact images use the first sector and the sector at logical offset `0x8000`. The reader resolves both through the selected boot table and respects skipped tables. An exact archive match is not evidence that the complete executable works on a different cart. [N-Card firmware archive](https://linfoxdomain.com/nintendo/ds/dl/NCard_v1.33beta.zip)

The 122G wrapper has a zero outer size field, but the known archive ends at `0xCA240`. Its dump plan requires all 51 mapped 16 KiB chunks and no longer falls back to an open-ended map walk. For reproducible tests with the original images, use `scripts/test-archived-firmware.sh`. Tests put the files behind fragmented primary and secondary mappings. Missing chunks, invalid pointers, skipped tables and modified fingerprints are rejected.

The NeoFlash MK5 1.45 auto-upgrader contains the same stock uDisk 1.45 image as the N-Card package. Removing the three-byte tags from its embedded records at file offset `0x24200` gives an exact 786,432-byte match (CRC32 `3DBEF02E`). This confirms a common runtime for those packages, not identical cart hardware or permission to bypass layout checks. [NeoFlash MK5 1.45 package](https://linfoxdomain.com/nintendo/ds/dl/MK5_Menu_v1.45_xMenu_v2.56.zip)

The native 1.45 installer also accepts both archived 122G images and the four compact N-Card releases above. It checks the complete source image on NAND and checks all 128 KiB of the old secondary map against the expected contiguous stock layout and footer. That is a stricter test than the read-only archive fingerprint. It requires independent storage for the backup and a verified active safe-mode route (patched on DSi). The installer verifies the complete saved backup before any erase, writes the new runtime, and publishes the secondary map last. The old 122G allocation beyond the new stock runtime is left intact. The tests use the original archive binaries on modeled NAND with both erase geometries. They do not establish that a real 122G or N-Card will have the modeled map.

## Match to real DS Linker spare data

All 72 bytes of the generated image's first table match Apache Thunder's 512-byte `shared_dslinker.bin` and both full before/after DS Linker spare dumps. The observed match is split:

| Generated file offset | Packed spare offset | Length |
| --- | --- | --- |
| `0x1000–0x1007` | `0xC0–0xC7` | 8 bytes |
| `0x1008–0x1047` | `0x80–0xBF` | 64 bytes |

This is evidence for where these 72 bytes came from on that DS Linker. It doesn't show that all block-0 contents can be regenerated, explain ARM7 handoff bytes across revisions, establish NAND offsets for NinjaPass Jr, or establish compatibility with YJLoader hardware. The comparison does not authorize a write.

## Reproducing Apache Thunder's main ARM9 edit

Emulating the same image-prep routine with the captured ExpressCard logical ROM reproduced all 2,048 ciphertext bytes at raw main `0x800–0xFFF` from the pre-edit physical backup. It also reproduced the 4 KiB table at main `0x1000–0x1FFF` exactly in this backup. The original client put those ranges at *logical file* `0x4000–0x47FF` and `0x2000–0x2FFF`, respectively. For these dumps, raw main `0x1008` falls in the table and is not the marker location.

Feeding the logical ROM that zeroes `0x4008–0x400F` back through the client produced just eight different ciphertext bytes in the matching 2 KiB window, corresponding to raw main `0x808–0x80F`. Its secure-area CRC was `0x3682`, matching the value in the edited ROM that Apache reported booting. The client rewrites unrelated fields in its output header and produces a different secure-area CRC when run on the earlier captured ROM. For the patch, use the exact verified ciphertext delta, not the generated header wholesale. Keep the live header except for the secure-area CRC and the recalculated header CRC.

nrioTool's **Advanced NAND tools → Patch main ARM9 marker** performs only this operation. It checks the recognized ExpressCard-style header, full ciphertext window, key table, and intact ARM7 spare copies on the live cart. It saves a restorable block-0 backup to separate storage, rechecks the block after confirmation, and verifies the rewritten main and spare. It does not import a replacement ROM, change capacity or branding, or fix 3DS detection. Other layouts stop before writing. The path was tested on a simulated NAND using Apache's raw capture. The new writer itself has not been tested on physical hardware.

## Read-only use

On the console, **Check Cart → Boot tables** distinguishes a direct-boot DS ROM header at `0x40000` from a secondary table and **NAND and early boot** reports when the known 56-byte overlap repeats in spare pages 2 and 3. Neither is a bootability or exact revision test.

On the PC, run:

```sh
python3 raw-nand/inspect_dsflash2.py candidate.nds --main main.bin --spare spare.bin
```

The optional main dump must be physical NAND data without interleaved spare. A block-0-only dump may not reach the address that the primary map points to. To read the usual secondary header and the start of a payload at `0x80000`, use an export that extends at least through `0x80200`. The script checks file-header and secure-area CRCs, compares the split table if present, and reports where the boot tables point. It never modifies the image or dump.
