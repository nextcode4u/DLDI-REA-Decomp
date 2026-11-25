# Unified NRIO DLDI

nrioTool's File Browser, setup actions, read-only Check Cart, and optional USB mount use the same C filesystem driver. Check Cart removes write support from its relocated copy. Startup and reads do not erase duplicate pending blocks in either mode. The standalone DLDI advertises read/write support.

Startup requires repeatable supported NAND IDs. It checks the candidate physical map's footer, addresses, and duplicate entries before enabling filesystem I/O. It can try a later candidate if an earlier one fails. When loading a zone again, it checks that the map still matches the one validated at startup. The writable mode bounds logical requests and reports failures returned by the FTL. Unknown or unvalidated NAND does not get writable access. ROMCTRL and NAND-ready waits are bounded. A transport failure disables both reads and writes until fresh startup rather than continuing an incomplete transfer. There is no new 4 GiB support.

This does not change the driver installed inside stock firmware or the separate XuluMenu application. The raw boot and spare-area tools still use physical NAND I/O rather than filesystem DLDI. The historical matching build and the earlier read-only test build remain available for comparison.

## Build and test

From `n-card/` with devkitARM r20:

```sh
make DEVKITPRO=/path/to/r20-sdk UNIFIED=1
make DEVKITPRO=/path/to/r20-sdk verify
sh scripts/test-format.sh
```

The host tests require a C compiler, Python with Unicorn, `mcopy` from mtools, and `arm-none-eabi-nm` (or set `NM` to its path). They check format fault handling, alternate maps, duplicate entries, and write errors. The round-trip test formats simulated NAND, then uses the relocated ARM binary to write a FAT file and read it back after a fresh startup. It covers both erase geometries, aligned and unaligned buffers, writes across a zone boundary, and the final sector. The round-trip test simulates NAND at the I/O boundary. The fault test drives the actual Slot-1 register polling code with simulated busy and short transfers, including verifying no program-confirm command is issued after a short write. The new writable build still needs a physical-cart test.

The build produces `build/unified/nrio.dldi`. The integration packages the checked-in `nrio-unified.dldi` so nrioTool builds don't require a second compiler. When refreshing that file, also update `DLDI_SHA` in `../nrioTool/embed_bundle.py`.
