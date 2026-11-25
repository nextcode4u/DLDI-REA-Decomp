#include "state.h"

extern int nrio_read_nand_id(u32 *id);
extern int nrio_init_nand_geometry(u32 block_size, u16 blocks, u32 data_base, u32 mode);

#ifdef NRIO_EXPERIMENTAL
#include "geometry.h"

NRIO_ROUTINE(34ec) bool nrio_ftl_init(u32 mode)
{
    u32 id, previous = 0;
    struct nrio_geometry geometry;
    nrio_sector_capacity = 0;
#ifdef NRIO_UNIFIED
    nrio_cached_logical_page = ~0u;
    nrio_bus_fault = 0;
#endif
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        if (!nrio_read_nand_id(&id) || !nrio_decode_geometry(id, &geometry)) {
            previous = 0;
            continue;
        }
        if (id == previous)
            return nrio_init_nand_geometry(geometry.pages_per_block << 11, geometry.data_blocks,
                                           geometry.reserved_bytes, mode) == 0;
        previous = id;
    }
    return false;
}

#else
NRIO_ROUTINE(34ec) void nrio_ftl_init(u32 mode)
{
    union {
        u32 word;
        u8 bytes[4];
    } id;

    nrio_read_nand_id(&id.word);
    if (id.bytes[0] != 0xec && id.bytes[0] != 0xad &&
        id.bytes[0] != 0x2c && id.bytes[0] != 0x98) {
        for (;;) {
        }
    }

    switch (id.bytes[1]) {
    case 0xc3:
    case 0xd3:
        if (id.bytes[3] & 0x20) {
            u32 flash_base = 0x1000000;
            u32 block_count = 0x0fc0;
            u32 block_size = 0x40000;
            nrio_init_nand_geometry(block_size, block_count, flash_base, mode);
        return;
        }
        nrio_init_nand_geometry(0x20000, 0x1fc0, 0x0800000, mode);
        return;
    case 0xc5:
    case 0xd5:
        if (id.bytes[3] & 0x20) {
            nrio_init_nand_geometry(0x40000, 0x1fc0, 0x1000000, mode);
        return;
        }
        nrio_init_nand_geometry(0x20000, 0x3fc0, 0x0800000, mode);
        return;
    case 0x71:
    case 0xca:
    case 0xda:
        if (id.bytes[3] & 0x20) {
            nrio_init_nand_geometry(0x40000, 0x03c0, 0x1000000, mode);
        return;
        }
        nrio_init_nand_geometry(0x20000, 0x07c0, 0x0800000, mode);
        return;
    default:
        for (;;) {
        }
    case 0xcc:
    case 0xdc:
        if (id.bytes[3] & 0x20) {
            nrio_init_nand_geometry(0x40000, 0x07c0, 0x1000000, mode);
        return;
        }
        nrio_init_nand_geometry(0x20000, 0x0fc0, 0x0800000, mode);
        return;
    case 0x74:
    case 0x79:
    case 0xc1:
    case 0xf1:
        if (id.bytes[3] & 0x20) {
            nrio_init_nand_geometry(0x40000, 0x01c0, 0x1000000, mode);
        return;
        }
        nrio_init_nand_geometry(0x20000, 0x03c0, 0x0800000, mode);
        return;
    }
}

#endif
