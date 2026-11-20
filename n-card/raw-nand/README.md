# N-Card physical NAND recovery

Use [nrio-raw-readonly.nds](compiled/nrio-raw-readonly.nds) for the first test. It is Apache Thunder's `nrio-usb-disk` fork with the recovery DLDI already embedded. With an N-Card in Slot-1 and a DS Linker Writer in Slot-2, it should report the NAND capacity and expose a read-only USB disk. If the computer offers to initialize or format the disk, cancel and use a disk imaging tool to save a full image. The first 512 bytes in the image should come from physical NAND address zero.

| Build | Standalone DLDI | USB app |
| --- | --- | --- |
| Read-only dump | [nrio-raw-readonly.dldi](compiled/nrio-raw-readonly.dldi) | [nrio-raw-readonly.nds](compiled/nrio-raw-readonly.nds) |
| Read-only spare dump | [nrio-raw-spare.dldi](compiled/nrio-raw-spare.dldi) | [nrio-raw-spare.nds](compiled/nrio-raw-spare.nds) |
| Spare recovery write | [nrio-raw-spare-write.dldi](compiled/nrio-raw-spare-write.dldi) | [nrio-raw-spare-write.nds](compiled/nrio-raw-spare-write.nds) |
| Main-area recovery write | [nrio-raw-recovery.dldi](compiled/nrio-raw-recovery.dldi) | [nrio-raw-recovery.nds](compiled/nrio-raw-recovery.nds) |

## Physical addressing

USB sector `n` maps to NAND main-data byte offset `n * 512`. The driver queries the chip ID for capacity and erase geometry. It bypasses the original block-table discovery, FAT volume offset, and logical block remapping. In the stock driver, `nrio_block_table_address` locates the FTL's physical block map, and the separate `nrio_nand_address` guards block low-address programming. The recovery driver uses explicit physical row and column addresses, including row zero, and checks requests against the detected chip capacity.

The USB image contains all addressable main data, including boot and other normally hidden regions. The 64 spare bytes per page are handled internally during writes. The 512-byte USB sector interface exposes main data only.

## Spare-area dump

The spare-area USB build packs eight consecutive 64-byte spare records into each 512-byte USB sector. The first 64 bytes in the dump belong to NAND page 0. For page `n`, the record starts at offset `n * 64` in the spare dump and its corresponding main data starts at `n * 2048` in the main dump. A 2 GiB N-Card should expose **131,072 USB sectors (64 MiB)** in the spare-area build. The exposed records include ECC and other spare bytes as returned by the N-Card bridge. Their contents can be compared between a working card and the damaged card. The location of the missing boot data remains unconfirmed.

## Spare recovery writes

The writable spare build uses the same eight-record layout as the read-only spare dumper. Every 512-byte USB write supplies eight consecutive 64-byte spare records. The driver reads the whole affected erase block, replaces only the requested spare records, rewrites the block, and compares the readback of all main and spare bytes with the staged data. It preserves the supplied ECC and marker bytes verbatim. Use spare data intended for the exact main-page contents on the target card. Copying spare bytes between cards can also copy block markers and other metadata.

Save main and spare dumps before writing. The same erase-block, bad-marker, and power-loss limitations described below apply. Spare writes remain unverified on physical hardware.

## Recovery writes

Take and retain a full read-only image before using the write-enabled build. Use physical offsets and an image appropriate for the target N-Card. A USB write can erase and rewrite an entire NAND erase block, even for a single changed sector. The driver stages that block in RAM supplied by the USB app, retains untouched data and spare bytes, updates NRIO ECC for changed sectors, and checks the resulting data and spare bytes by reading the block back. It stops further writes after a failure until driver startup. A request spanning two erase blocks can commit the first block before encountering an error in the second. It rejects unfamiliar spare markers in the first two pages of a block. The path has no remapping or power-loss recovery. Boot-region ECC and any hardware write protection still need testing on a physical card.

## USB integration and rebuild

Stock `nrio-usb-disk` gets disk capacity from a FAT or MBR header. The patches query the `NRWR` driver for physical capacity and bind the erase-block buffer instead. They also report write protection for the read-only variant and reject out-of-range USB requests before calling DLDI. The private capacity and buffer commands are only called by the USB app during initialization. The prebuilt USB apps require `NRWR` and have the first embedded DLDI magic byte changed from `0xED` to `0xEE` so a launcher using standard DLDI signature matching leaves the embedded driver in place. The standalone DLDIs retain standard magic for patching.

- [Apache Thunder fork patch](usb/fork.patch), built against [b9b781b](https://github.com/ApacheThunder/nrio-usb-disk/tree/b9b781bf5deec5096cbc517432878430ac0e58c6).
- [Asie upstream patch](usb/upstream.patch), for [58dbb2d](https://github.com/asiekierka/nrio-usb-disk/tree/58dbb2dba8b49e1cf4c25e26c906bafb3228bace).

Build the standalone drivers from this folder with `make DEVKITPRO=/path/to/r20-sdk`. Apply the matching USB patch in a checkout of the USB app, initialize its submodules if applicable, and build it with BlocksDS using `make NRIO_RAW_REQUIRED=1`. Make one copy of the resulting NDS per variant and run `dldipatch patch DRIVER.dldi COPY.nds`. To reproduce the launcher protection, locate the embedded DLDI magic with ID `NRWR` at header offset `0x60` and change its first byte to `0xEE`. [Third-party notices](compiled/licenses/)

## Verification

`python3 check_transport.py` runs the relocated DLDI extracted from each compiled NDS in Unicorn. It checks sector zero, the final sector, out-of-range rejection, both erase geometries, write gating, preservation of adjacent data and spare bytes across erase-block boundaries, and bad-marker refusal against a simulated NAND bridge. It also injects erase failures, program failures, and readback mismatches to check that the driver rejects later writes while allowing reads. The spare-area tests also check eight-record packing, capacity, the final physical page, read-only write rejection, exact spare replacement across erase blocks, main-data preservation, and failure handling. Spare-area reads remain unverified on physical hardware.

## Hardware results

HxD on Windows showed the USB disk with 4,194,304 sectors of 512 bytes each (2 GiB) and displayed data starting at physical sector 0. The screenshot also shows `DSGBA PA` at byte offset `0x400` (sector 2). This confirms USB enumeration and reads at the start of NAND on the tested card. A full dump, reads at the end of the device, and physical recovery writes remain to be verified.
