# uDisk BIOS updater ARM9 reconstruction

This folder contains readable C for the NAND address check, erase command encoding, block-map expansion, and byte-sum verification routines in the recovered stock BIOS updater. The separate installed uDisk runtime that provides USB storage has its own ARM9 program. [Recovered stock updater](https://github.com/m4x10187/fire-card-archive/tree/ced86434054e47dcca8d4e90efd25cc92f88d688/N-Card/Dumps/recovered)

The reference is `update.nds` with SHA-256 `3391af866e42057576c2c9841cf748242612fde77bd3853fceaecaaea482ca11`. Its ARM9 starts at file offset `0x200` and loads at `0x02000000`.

| ARM9 address | C routine | Behavior |
| --- | --- | --- |
| `0x0200042C` | `udisk_update_address_allowed` | Checks the lower and upper physical NAND bounds used by the updater |
| `0x020011A0` | `udisk_erase_frames` | Returns the command words the original routine sends, with the optional bank command |
| `0x020007B8–0x020007E8` | `udisk_expand_block_map` | Expands 2,048 allocation words into a 64 KiB little-endian map, including the wraparound values from unused entries |
| `0x020003D8` | `udisk_byte_sum` | Adds main-area bytes for the stock write/readback comparison |

The C module has no Nintendo DS library dependency. Compile it from this directory with a C11 compiler.

```sh
cc -std=c11 -Wall -Wextra -Werror -c updater.c
```

The code also compiled for ARM9 with devkitARM. A host comparison against the original ARM9 emulated 44 combinations of addresses, erase geometries and bank flags. The guard results, complete erase command sequences, all 65,536 map bytes, and sums for the tested aligned input lengths matched. Frame generation returns data to the caller and has no hardware side effects.

The reconstruction covers those four routines. The rest of the updater, the runtime USB stack, and cartridge factory provisioning remain open. [NAND recovery findings](../raw-nand/README.md#ecc-and-the-original-udisk-package)
