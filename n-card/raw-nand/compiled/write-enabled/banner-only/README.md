# Banner-only N-Card recovery

The [USB app template](nrio-raw-banner.nds) and [standalone DLDI template](nrio-raw-banner.dldi) ship unarmed. The USB app exposes the main NAND data read-only until you prepare a copy for a particular backup and replacement. Use the [read-only dump tools](../../../README.md) to save both the main and spare areas of the target before proceeding.

Prepare three files:

| File | Required bytes |
| --- | --- |
| `target-first-eight.spare` | The target card's spare dump at `0x000–0x1FF`, exactly 512 bytes |
| `banner.bin` | The new page 4 main data for `0x2000–0x27FF`, exactly 2,048 bytes |
| `banner-page4.spare` | The intended page 4 spare record, exactly 64 bytes. In a spare-only dump this record is at `0x100–0x13F`. |

Use a replacement main page and spare record verified to work together with the target boot layout. The tool uses the supplied spare bytes verbatim, including any ECC and boot metadata. The firmware's exact assignments within this record remain unconfirmed.

Run the [arming tool](../../../arm_banner.py) with Python 3:

```sh
python3 arm_banner.py nrio-raw-banner.nds \
  --target-spare target-first-eight.spare \
  --banner-main banner.bin \
  --banner-spare banner-page4.spare \
  --output nrio-raw-banner-armed.nds
```

Boot the armed copy and submit `banner.bin` as one 2,048-byte USB write starting at byte offset `0x2000`. The DS rejects other ranges, sizes, or payloads. Before erasing, it compares all eight saved spare records against the target. It rewrites physical erase block 0 with the intended page 4 main and spare bytes, preserves the staged main and spare bytes of every other page, and verifies the entire block by reading it back.

The write still erases a boot-critical block. Loss of power or a hardware erase or program failure can leave the card unbootable. The banner-only build has passed emulation. Physical writes with this build remain untested.
