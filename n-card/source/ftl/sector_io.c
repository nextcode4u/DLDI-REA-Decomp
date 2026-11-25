#include "state.h"

extern int nrio_write_sector_with_mapping(void);
extern void nrio_read_sector_with_mapping(void);

NRIO_ROUTINE(30ec) void nrio_ftl_read(u32 offset, u32 buffer_address, u32 length)
{
    nrio_new_sector_request = 1;
    nrio_sector_count = length >>= 9;
    nrio_current_sector = offset >>= 9;
#ifdef NRIO_USB_MOUNT
    if (nrio_current_sector >= nrio_sector_capacity ||
        length > nrio_sector_capacity - nrio_current_sector) {
#else
    if (offset + 1 > nrio_sector_capacity) {
#endif
        return;
    }

    for (u32 index = 0; index < length; ++index) {
        nrio_read_buffer = (u8 *)buffer_address;
        nrio_read_sector_with_mapping();
#ifdef NRIO_UNIFIED
        if (!nrio_sector_capacity) return;
#endif
        ++nrio_current_sector;
        buffer_address += 512;
    }
}

#ifdef NRIO_UNIFIED
#define WRITE_RESULT bool
#else
#define WRITE_RESULT void
#endif
NRIO_ROUTINE(4978) WRITE_RESULT nrio_ftl_write(u32 offset, const void *input, u32 length)
{
#ifndef NRIO_USB_MOUNT
    if (nrio_current_sector + 1 > nrio_sector_capacity) {
        return;
    }
#endif

    nrio_cached_logical_page = 0xffffffff;
    u32 sector = offset >> 9;
    u32 count = length >> 9;
    u32 retry;
#ifdef NRIO_UNIFIED
    unsigned attempts = 3;
    /* The card FIFO and ECC routine expect aligned words and may prefetch
       beyond the sector. Keep that lookahead inside an owned buffer. */
    u32 aligned[130];
    aligned[128] = aligned[129] = ~0u;
    if (!input || !count || sector >= nrio_sector_capacity ||
        count > nrio_sector_capacity - sector) return false;
#endif
    do {
        nrio_current_sector = sector;
        nrio_sector_count = count;
        nrio_new_sector_request = 1;
#ifndef NRIO_UNIFIED
        if (count == 0) {
            return;
        }
#endif

        retry = 0;
        const u8 *buffer = input;
        int index = 0;
        do {
#ifdef NRIO_UNIFIED
            if (((u32)buffer & 3u) || (u32)index == count - 1) {
                u8 *dst = (u8 *)aligned;
                for (unsigned b = 0; b < 512; ++b) dst[b] = buffer[b];
                nrio_write_buffer = dst;
            } else {
                nrio_write_buffer = buffer;
            }
            int status = nrio_write_sector_with_mapping();
            if (!nrio_sector_capacity) return false;
            if (status == 0x10) retry = 1;
            else if (status != 0) return false;
#else
            nrio_write_buffer = buffer;
            if (nrio_write_sector_with_mapping() == 0x10) {
                retry = 1;
            }
#endif
            ++index;
            buffer += 512;
        } while ((u32)index != count);
#ifdef NRIO_UNIFIED
    } while (retry && --attempts);
    return !retry;
#else
    } while (retry);
#endif
}
