#include "state.h"

extern int nrio_write_sector_with_mapping(void);
extern void nrio_read_sector_with_mapping(void);

NRIO_ROUTINE(30ec) void nrio_ftl_read(u32 offset, u32 buffer_address, u32 length)
{
    nrio_new_sector_request = 1;
    nrio_sector_count = length >>= 9;
    nrio_current_sector = offset >>= 9;
    if (offset + 1 > nrio_sector_capacity) {
        return;
    }

    for (u32 index = 0; index < length; ++index) {
        nrio_read_buffer = (u8 *)buffer_address;
        nrio_read_sector_with_mapping();
        ++nrio_current_sector;
        buffer_address += 512;
    }
}

NRIO_ROUTINE(4978) void nrio_ftl_write(u32 offset, const void *input, u32 length)
{
    if (nrio_current_sector + 1 > nrio_sector_capacity) {
        return;
    }

    nrio_cached_logical_page = 0xffffffff;
    u32 sector = offset >> 9;
    u32 count = length >> 9;
    u32 retry;
    do {
        nrio_current_sector = sector;
        nrio_sector_count = count;
        nrio_new_sector_request = 1;
        if (count == 0) {
            return;
        }

        retry = 0;
        const u8 *buffer = input;
        int index = 0;
        do {
            nrio_write_buffer = buffer;
            if (nrio_write_sector_with_mapping() == 0x10) {
                retry = 1;
            }
            ++index;
            buffer += 512;
        } while ((u32)index != count);
    } while (retry);
}
