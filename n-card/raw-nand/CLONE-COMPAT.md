# N-Card clone compatibility findings

Cart labels aren't enough to pick an installer. Some DS Linker and Ultra
FlashPass EX packages contain identical firmware. Two Fire Link hardware dumps
instead use materially different secondary maps even though both have a uDisk
1.45-style loader.

| Family | What nrioTool can do |
| --- | --- |
| DS Linker 1.22G, 1.22N, 1.23A | Recognize and dump known wrappers. Install stock uDisk 1.45 only with an exact image and standard secondary map. |
| DS Linker 1.40 | Recognize and dump the compact runtime. Stock upgrade accepts the complete method 2 image. Method 1 has a 3-byte record suffix for each 512 data bytes. Removing those suffixes gives exactly the same declared runtime as method 2 through `0x90440`, but not the same captured end padding. |
| Ultra FlashPass EX 1.21H, 1.23, 1.23A | Recognize and dump the known images. 1.21H uses four 128 KiB firmware allocations instead of six. Upgrade checks that the two additional allocations are all `FF` in both main and spare, then repeats that check after confirmation. The tagged 1.23 updater and the 1.23A writer-ready files have separate checksums. |
| DS Fire Link 1.23 multi-updater | Its 18 embedded wrappers resolve to six unique images. They share header families with UFPEX 1.23, but their contents differ. Their exact images can qualify for a stock upgrade under the same map checks. |
| DS Fire Link 1.23o and early Fire Link dump | Read-only recognition and SRL dumping. The separate 1.23o updater contains a tagged `0x118240` wrapper. The early hardware dump contains a different `0x6ec40` wrapper. Neither was used to establish a permitted write layout. |
| N-Card stock 1.33 beta, 1.41, 1.43, 1.44 | Existing exact archive upgrade paths remain available. |
| Single-game cart | Check Cart reports the repeated `<<GAME>>` marker observed at `0x40000` on one clone instead of treating it as a secondary table. A marker doesn't authorize writes. |

The archive upgrade check validates the whole recognized source image and
the whole canonical secondary map, including its footer. It requires an
independent backup destination, preserves the primary boot route and commits
the secondary map last. In DSi mode it also requires the cart's safe mode to
be patched. The two diagnostic fingerprints alone cannot authorize an upgrade.

## Remaining gaps and diagnostics

- **Fire Link 8 Gbit** has a revision-1 secondary footer, map CRC32
  `AF0A3FDB`, and 512 contiguous entries covering NAND `0x80000–0x87FFFF`.
  The normal 1.45 runtime fits in six allocations, but the purposes of the extra
  entries are unverified. Check Cart reports the match. Bypass and firmware
  replacement still refuse this layout.
- **Fire Link 16 Gbit** has the six-allocation map (CRC32 `D9D5E30A`). Its
  entire declared uDisk 1.45 runtime matches the archive. One bit differs in
  padding at NAND `0x13FFFC`, making the full image CRC32 `8502974B`.
  Bypass safe mode accepts that exact map and image. It only redirects the
  primary boot table, and rechecks the source after confirmation. No runtime
  rewrite is enabled by this exception. Both sizes were verified against the
  original captures, with writes simulated at both supported erase geometries.
- **Early FPGA DS Linker.** A newer safe mode can show its screen and fail to
  hand off to uDisk, while a patch to the cart's own safe mode boots. Check Cart
  now fingerprints the live safe-mode ARM9 independently of the banner and
  ARM7. In the supplied archive, two DS Linker ROM pairs had identical ARM9
  bytes before and after patching within each pair, but different ARM9 hashes
  between pairs (`DCA3E6FC` and `FB3145A0`). The stock ARM7 hashes differ too.
  This helps identify the ROM and compare results. It does not prove which
  command or timing causes the FPGA failure. Test the cart's own ROM first.
- **Ultra N-Card 1.23o.** The original May 2007 listing is still online, but its
  file link redirects to FileTrip and the downloaded responses were HTML, not
  an archive. No distinct image or layout is verified. Don't assume it is the
  same image as Fire Link 1.23o. [Original upload listing](https://gbatemp.net/download/ultra-n-card-card-update.458/)
- **NinjaPass Jr.** A contemporary account reports that the NinjaPass download
  was called `dsflash2.exe` and could write an Ultra FlashPass. The same source
  is explicitly unsure about the Junior board. A NinjaPass-branded `DSGB` ROM
  can be labeled by the offline tool, but this won't prove which NAND map it
  uses. [Contemporary client report](https://ayasuke.exblog.jp/5409793/)
- **YJLoader.** Apache's documented board uses `YJYJ` and separate SPI save
  storage. The offline inspector recognizes valid `YJLOADER` or `YJMENU`
  headers as possible YJLoader firmware, not a DSFlash2 write image. Identify
  this separately rather than using the NRIO writer to probe it.
- **32 Gbit (4 GiB).** The read-only probe now requires two stable, different
  upper responses that each differ from their lower mirror before it reports
  that upper addresses differ. A repeated nonblank controller response is
  inconclusive. This still needs the physical chip part number and a hardware
  run. No DLDI mounting, NAND geometry, or writes have been enabled.

Archive sources are the [linFox index](https://linfoxdomain.com/nintendo/ds/),
[Fire Card hardware archive](https://github.com/m4x10187/fire-card-archive),
and [Apache Thunder's YJLoader hardware notes](https://gbatemp.net/threads/mystery-flashcart-used-by-dora-bootleg.656146/).
Firmware images are test inputs, not checked into this directory.

## Apache Thunder's NSMB bootleg backup

The 128 KiB main, 4 KiB packed spare, and `backup.info` match the manifest
CRCs. The manifest identifies NAND `0x1D80F1AD` and a 64-page erase block. Its
primary table has 61 contiguous entries covering the declared ROM at
`0x40000`. The header in the manifest's `0x40000` sample passes its DS header
CRC and has sector CRC32 `4387AAEB`, matching the known XuluMenu 1.4.1 header.
This establishes the captured boot route and header, not a byte-for-byte
match of the full runtime. The physical block-0 dump does not reach the ROM.

Spare page 0 is all `FF`. Pages 1, 4, and 5 carry the same ARM7 record with
handoff `0x1918027E`. Spare pages 2 and 3 match the shared records seen on
larger cards. That confirms those fields also occur on a 64-page sample, but
does not identify a universal replacement for a wiped block 0.

The updated `CHECK-CART.TXT` has two entries. The older one is for a
different cart (`0x2514D3EC`, 128 pages, safe mode at `0x20000`). The newer
entry matches this backup's NAND ID, erase size and primary ROM at
`0x40000`. It reports 262,144 physical sectors (128 MiB), a read-only FAT
mount and stock `xmenu.dat` on the filesystem. That is consistent with
hidden XuluMenu and stock XMenu coexisting. Its secondary “Unknown
layout” is the first ARM instruction of that primary ROM being
interpreted as a map word by the older build. The report display now
treats this address as the primary ROM and skips the secondary table probe. The capture ends partway
through the report, so it does not include the surviving-window checksums.
