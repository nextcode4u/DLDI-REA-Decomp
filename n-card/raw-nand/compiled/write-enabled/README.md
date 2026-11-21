# Write-enabled N-Card recovery builds

Use [nrio-raw-toolkit.nds](nrio-raw-toolkit.nds) with its embedded multi-mode driver. The [DS mode menu](../../README.md#ds-mode-menu) starts read-only and requires confirmation for writes. It also includes an [on-console boot-table repair](../../README.md#repair-using-only-the-target-cart) for supported carts with a surviving uDisk mapping. A [standalone DLDI](nrio-raw-toolkit.dldi) is available for patching.

These builds can erase and rewrite physical NAND. A wrong image or offset, a failed program operation, or a power loss during an erase-block rewrite can leave the card unbootable. Save both main and spare dumps before writing.

The [banner mode](banner-only/) accepts a single page-4 write when the toolkit ROM is prepared with matching target spare records and the exact replacement payload.

The protected mode rejects writes touching the first erase block. It can still rewrite later blocks, including blocks that may contain boot data. The main-area and spare-area views use different sector layouts. Check the offset mapping before writing. [Recovery documentation](../../README.md)
