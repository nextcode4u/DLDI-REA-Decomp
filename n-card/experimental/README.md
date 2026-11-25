# Experimental read-only NRIO DLDI

This build comes from the C reconstruction. The matching binary and the normal writable C driver remain separate.

- The driver tries up to four bounded NAND ID reads and requires two consecutive matching supported IDs. It searches the entire reserved table window and tries later table candidates when a map is invalid. If no supported ID or valid map is found, startup returns an error.
- The driver checks the capacity entry and mapped addresses before accepting the FAT volume. Sector requests are checked before conversion to byte offsets. Unaligned read buffers use an aligned bounce buffer.
- This variant rejects writes, including the legacy startup cleanup that could erase a duplicate pending block. It retains the 32 KiB DLDI slot and the `NRIO` device type.
- The geometry table still accepts only the original 128 MiB to 2 GiB profiles. Address arithmetic is tested up to the end of a 4 GiB space, but no 4 GiB mount or programming is enabled.

The first Check Cart builds bundled this variant. nrioTool now uses the [unified C driver](../unified/README.md) for both modes. On unsupported hardware, **SELECT** in Check Cart runs the separate 4 GiB raw address probe.

## Build and test

From `n-card/`, with the archived devkitARM r20 SDK available:

```sh
make DEVKITPRO=/path/to/r20-sdk EXPERIMENTAL=1
make DEVKITPRO=/path/to/r20-sdk verify
cc -std=c11 experimental/tests/geometry-test.c -o /tmp/nrio-geometry-test
/tmp/nrio-geometry-test
python3 experimental/tests/test_driver.py build/experimental/nrio.elf
```

The Python test uses Unicorn and `arm-none-eabi-nm` (or the path in `NM`). It runs the relocated binary with simulated NAND responses. The standalone build is `build/experimental/nrio.dldi`. The matching verification uses its own build directory and still compares byte for byte to the historical reference.

The packaged `nrio-readonly.dldi` has SHA-256 `c21727500805b72bbf48107672e1787a06ab17173f7b42d2b35d41d0839a37ad`.
