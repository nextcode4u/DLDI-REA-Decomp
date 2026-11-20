# N-Card physical NAND recovery

Use [nrio-raw-readonly.nds](compiled/nrio-raw-readonly.nds) for the first test. It is Apache Thunder's `nrio-usb-disk` fork with the recovery DLDI already embedded. With an N-Card in Slot-1 and a DS Linker Writer in Slot-2, it should report the NAND capacity and expose a read-only USB disk. If the computer offers to initialize or format the disk, cancel and use a disk imaging tool to save a full image. The first 512 bytes in the image should come from physical NAND address zero.

| Build | Standalone DLDI | USB app |
| --- | --- | --- |
| Read-only dump | [nrio-raw-readonly.dldi](compiled/nrio-raw-readonly.dldi) | [nrio-raw-readonly.nds](compiled/nrio-raw-readonly.nds) |
| Recovery write | [nrio-raw-recovery.dldi](compiled/nrio-raw-recovery.dldi) | [nrio-raw-recovery.nds](compiled/nrio-raw-recovery.nds) |

## Physical addressing

USB sector `n` maps to NAND main-data byte offset `n * 512`. The driver queries the chip ID for capacity and erase geometry. It bypasses the original block-table discovery, FAT volume offset, and logical block remapping. In the stock driver, `nrio_block_table_address` locates the FTL's physical block map, and the separate `nrio_nand_address` guards block low-address programming. The recovery driver uses explicit physical row and column addresses, including row zero, and checks requests against the detected chip capacity.

The USB image contains all addressable main data, including boot and other normally hidden regions. The 64 spare bytes per page are handled internally during writes. The 512-byte USB sector interface exposes main data only.

## Recovery writes

Take and retain a full read-only image before using the write-enabled build. Use physical offsets and an image appropriate for the target N-Card. A USB write can erase and rewrite an entire NAND erase block, even for a single changed sector. The driver stages that block in RAM supplied by the USB app, retains untouched data and spare bytes, updates NRIO ECC for changed sectors, and checks the resulting data and spare bytes by reading the block back. It stops further writes after a failure until driver startup. It rejects unfamiliar spare markers in the first two pages of a block. The path has no remapping or power-loss recovery. Boot-region ECC and any hardware write protection still need testing on a physical card.

## USB integration and rebuild

Stock `nrio-usb-disk` gets disk capacity from a FAT or MBR header. The patches query the `NRWR` driver for physical capacity and bind the erase-block buffer instead. They also report write protection for the read-only variant. The prebuilt USB apps require `NRWR` and have the first embedded DLDI magic byte changed from `0xED` to `0xEE` so a launcher using standard DLDI signature matching leaves the embedded driver in place. The standalone DLDIs retain standard magic for patching.

- [Apache Thunder fork patch](usb/fork.patch), built against [b9b781b](https://github.com/ApacheThunder/nrio-usb-disk/tree/b9b781bf5deec5096cbc517432878430ac0e58c6).
- [Asie upstream patch](usb/upstream.patch), for [58dbb2d](https://github.com/asiekierka/nrio-usb-disk/tree/58dbb2dba8b49e1cf4c25e26c906bafb3228bace).

Build the standalone drivers from this folder with `make DEVKITPRO=/path/to/r20-sdk`. Apply the matching USB patch in a checkout of the USB app, initialize its submodules if applicable, and build it with BlocksDS using `make NRIO_RAW_REQUIRED=1`. Make one copy of the resulting NDS per variant and run `dldipatch patch DRIVER.dldi COPY.nds`. To reproduce the launcher protection, locate the embedded DLDI magic with ID `NRWR` at header offset `0x60` and change its first byte to `0xEE`. [Third-party notices](compiled/licenses/)

## Verification

`python3 check_transport.py` runs the relocated DLDI extracted from each compiled NDS in Unicorn. It checks sector zero, the final sector, out-of-range rejection, both erase geometries, write gating, preservation of adjacent data, and bad-marker refusal against a simulated NAND bridge. This verifies driver execution in emulation. USB enumeration, NAND commands, and recovery writes remain unverified on physical hardware.
