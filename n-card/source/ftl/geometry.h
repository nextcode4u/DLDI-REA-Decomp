#ifndef NRIO_EXPERIMENTAL_GEOMETRY_H
#define NRIO_EXPERIMENTAL_GEOMETRY_H

#ifndef ARM9
#include <stdbool.h>
#endif
#include <stdint.h>

struct nrio_geometry {
    uint32_t physical_sectors;
    uint32_t reserved_bytes;
    uint16_t data_blocks;
    uint16_t pages_per_block;
};

/* Only the profiles recognized by the original filesystem driver. Larger
   chips still need a real probe and an independently validated FTL layout. */
static inline bool nrio_decode_geometry(uint32_t id, struct nrio_geometry *out)
{
    unsigned maker = id & 255, device = (id >> 8) & 255, scale;
    if (!out || (maker != 0xec && maker != 0xad && maker != 0x2c && maker != 0x98))
        return false;
    switch (device) {
        case 0x74: case 0x79: case 0xc1: case 0xf1: scale = 1; break;
        case 0x71: case 0xca: case 0xda: scale = 2; break;
        case 0xcc: case 0xdc: scale = 4; break;
        case 0xc3: case 0xd3: scale = 8; break;
        case 0xc5: case 0xd5: scale = 16; break;
        default: return false;
    }
    unsigned pages = id & (UINT32_C(0x20) << 24) ? 128 : 64;
    out->physical_sectors = scale << 18;
    out->reserved_bytes = pages << 17;
    out->data_blocks = (out->physical_sectors / (pages * 4)) - 64;
    out->pages_per_block = pages;
    return true;
}

/* 32-bit byte starts and lengths cross neither capacity nor the 4 GiB
   exclusive end. Do not add two unchecked values or silently truncate. */
static inline bool nrio_byte_range(uint32_t sector, uint32_t count, uint32_t capacity,
                                   uint32_t *offset, uint32_t *length)
{
    if (!offset || !length || !count || sector >= capacity || count > capacity - sector ||
        sector > (UINT32_MAX >> 9) || count > (UINT32_MAX >> 9) ||
        count > (UINT32_C(1) << 23) - sector) return false;
    *offset = sector << 9;
    *length = count << 9;
    return true;
}
#endif
