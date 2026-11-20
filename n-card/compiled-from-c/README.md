# NRIO built from C

The included [nrio.dldi](nrio.dldi) builds all 60 driver routines from C. The build assembles the DLDI header and data tables and links the devkitARM startup and division runtime. It was built from source commit `eb3d5ba0eb26a41c19579f24189e0f97ceb46cb1` with devkitARM r20, GCC 4.1.1, and libnds. The artifact passed 832 original differential ARM emulation comparisons and 32 additional write transport comparisons against the byte-matching build across two load addresses. The driver mounted an N-Card through GodMode9i and the benchmark reported about 4.2 MB/s for reads. The write benchmark stalled with both the C build and the stock driver. Hardware writes remain unverified.

| Binary | Size | SHA-256 |
| --- | ---: | --- |
| Compiled from C | 20,056 bytes | `fe5ae8880fd240828098efc37a6438b7a5994b3d31ac709e47d6eb60c08f56b4` |
| [Original XuluMenu reference](../reference/nrio.dldi) | 19,724 bytes | `716937fa83dda22df3b27bf2c08ce9ea9a0209c6d64498a83b527c0dddae8978` |

## Byte differences

The first difference is at file offset `0x0044`, where the DLDI header's data-end address changes from `0xBF804D0C` to `0xBF804E58`. The C output is 332 bytes larger. Comparing the files at equal offsets gives 14,814 differing bytes in the overlapping region, including bytes displaced by the layout changes. The table gives file offsets and spans up to the next routine, including any literal pools.

| Routine | Original offset | C offset | Original bytes | C bytes |
| --- | ---: | ---: | ---: | ---: |
| `nrio_read_nand_sector_with_ecc` | `0x0A5C` | `0x0A5C` | 876 | 908 |
| `nrio_write_nand_sector_with_ecc` | `0x0DC8` | `0x0DE8` | 1,388 | 1,408 |
| `nrio_page_operation` | `0x195C` | `0x1990` | 860 | 860 |
| `nrio_copy_nand_pages` | `0x1EC8` | `0x1EFC` | 316 | 324 |
| `nrio_load_zone` | `0x2228` | `0x2264` | 3,376 | 3,488 |
| `nrio_init_nand_geometry` | `0x31A8` | `0x3254` | 836 | 848 |
| `nrio_read_sector_with_ecc_mapping` | `0x3874` | `0x392C` | 836 | 836 |
| `nrio_write_sector_with_mapping` | `0x3FC0` | `0x4078` | 2,488 | 2,636 |

GCC emits different ARM instructions for these eight routines. Differences include register allocation, stack slots, instruction scheduling, and branch layout. The page dispatcher and mapped ECC read have the original sizes and still differ in instruction bytes.

The size changes shift later routines and sections. Link-time values also change, including branch displacements, addresses in the global offset table, and the header's section boundaries and six driver entry points. The diagnostic strings and linked division runtime match at their respective offsets in both files.

## Reproduce and compare

Run these commands from `n-card/` with the historical SDK installed. [SDK setup](../README.md#build-and-compare)

```sh
make DEVKITPRO=/path/to/r20-sdk
cmp compiled-from-c/nrio.dldi build/nrio.dldi
sha256sum compiled-from-c/nrio.dldi reference/nrio.dldi
make C_ONLY=0 DEVKITPRO=/path/to/r20-sdk verify
python3 scripts/check_c_behavior.py
```

The behavior comparison requires Python and Unicorn 2.1.4. Run `cmp -l reference/nrio.dldi compiled-from-c/nrio.dldi` to list the differing bytes at equal file offsets. To inspect the original and C routine addresses, run the SDK's `arm-eabi-nm -n` on `build/matching/nrio.elf` and `build/nrio.elf`. The first link address for both is `0xBF800000`. Subtract it from each symbol address to get a file offset.
