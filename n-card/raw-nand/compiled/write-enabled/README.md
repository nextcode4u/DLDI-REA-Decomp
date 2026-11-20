# Write-enabled N-Card recovery builds

These builds can erase and rewrite physical NAND. A wrong image or offset, a failed program operation, or a power loss during an erase-block rewrite can leave the card unbootable. Save both main and spare dumps using the [read-only builds](../../README.md) before using any binary in this folder.

| Build | USB app | Standalone DLDI |
| --- | --- | --- |
| Main-area write with block zero protected | [nrio-raw-protected.nds](nrio-raw-protected.nds) | [nrio-raw-protected.dldi](nrio-raw-protected.dldi) |
| Main-area recovery write | [nrio-raw-recovery.nds](nrio-raw-recovery.nds) | [nrio-raw-recovery.dldi](nrio-raw-recovery.dldi) |
| Spare-area recovery write | [nrio-raw-spare-write.nds](nrio-raw-spare-write.nds) | [nrio-raw-spare-write.dldi](nrio-raw-spare-write.dldi) |

The protected build rejects writes touching the first erase block. It can still rewrite later blocks, including blocks that may contain boot data. The main-area and spare-area views use different sector layouts. Check the offset mapping before writing. [Recovery documentation](../../README.md)
