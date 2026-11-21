# Write-enabled N-Card recovery builds

Use [nrio-raw-toolkit.nds](nrio-raw-toolkit.nds) with its embedded multi-mode driver. The [DS mode menu](../../README.md#ds-mode-menu) starts read-only and requires confirmation for writes. It also includes an [on-console boot-table repair](../../README.md#repair-using-only-the-target-cart) for supported carts with a surviving uDisk mapping. A [standalone DLDI](nrio-raw-toolkit.dldi) is available for patching.

These builds can erase and rewrite physical NAND. A wrong image or offset, a failed program operation, or a power loss during an erase-block rewrite can leave the card unbootable. Save both main and spare dumps before writing.

## Previous single-mode builds

| Build | USB app | Standalone DLDI |
| --- | --- | --- |
| Main-area write with block zero protected | [nrio-raw-protected.nds](nrio-raw-protected.nds) | [nrio-raw-protected.dldi](nrio-raw-protected.dldi) |
| Main-area recovery write | [nrio-raw-recovery.nds](nrio-raw-recovery.nds) | [nrio-raw-recovery.dldi](nrio-raw-recovery.dldi) |
| Spare-area recovery write | [nrio-raw-spare-write.nds](nrio-raw-spare-write.nds) | [nrio-raw-spare-write.dldi](nrio-raw-spare-write.dldi) |

A separate [banner-only build](banner-only/) accepts a single page-4 write when prepared with matching target spare records and the exact replacement payload.

The protected build rejects writes touching the first erase block. It can still rewrite later blocks, including blocks that may contain boot data. The main-area and spare-area views use different sector layouts. Check the offset mapping before writing. [Recovery documentation](../../README.md)
