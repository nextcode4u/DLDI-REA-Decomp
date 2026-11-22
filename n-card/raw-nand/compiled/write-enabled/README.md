# Write-enabled N-Card recovery builds

Use [nrio-raw-toolkit.nds](nrio-raw-toolkit.nds) for the [DS mode menu](../../README.md#ds-mode-menu) and [on-console boot-table repair](../../README.md#repair-using-only-the-target-cart). It starts read-only and requires confirmation for writes. The [standalone raw DLDI](nrio-raw-toolkit.dldi) provides read-only access to physical main-area sectors for applications that use the standard DLDI interface.

These builds can erase and rewrite physical NAND. A wrong image or offset, a failed program operation, or a power loss during an erase-block rewrite can leave the card unbootable. Save both main and spare dumps before writing.

The [banner mode](banner-only/) accepts a single page-4 write when the toolkit ROM is prepared with matching target spare records and the exact replacement payload.

The [stock safe-mode installer](../../README.md#targeted-stock-safe-mode-install) uses this build as a template. The desktop toolkit prepares a private copy with the target's full erase-block snapshot. The install path has passed emulation and still needs hardware testing.

The protected mode rejects writes touching the first erase block. It can still rewrite later blocks, including blocks that may contain boot data. The main-area and spare-area views use different sector layouts. Check the offset mapping before writing. [Recovery documentation](../../README.md)
