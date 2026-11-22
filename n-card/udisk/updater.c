#include "updater.h"

/* ARM9 0x0200042C. The upper bound uses the detected erase-block size. */
bool udisk_update_address_allowed(uint32_t address, uint32_t erase_block_bytes)
{
    return address >= 0x40000 && address <= UINT32_C(0xA00000) - erase_block_bytes;
}

/* ARM9 0x020011A0. The original sends each word most significant byte first
   and uses a zero second word. This reconstruction returns the words for inspection. */
size_t udisk_erase_frames(uint32_t address, uint32_t erase_block_bytes, bool send_bank,
                         uint32_t frames[UDISK_MAX_ERASE_FRAMES])
{
    if (!udisk_update_address_allowed(address, erase_block_bytes))
        return 0;

    uint32_t row = (address >> 11) & 0xffff;
    size_t count = 0;
    frames[count++] = UINT32_C(0xD1616000);
    frames[count++] = UINT32_C(0xD1520000) | ((row & 0xff) << 8) | (row >> 8);
    if (send_bank)
        frames[count++] = UINT32_C(0xD1510000) | ((address >> 27) << 8);
    frames[count++] = UINT32_C(0xD161D000);
    return count;
}

/* ARM9 0x020007B8–0x020007E8. The stock allocator initializes unused words
   to all ones. The expansion applies the same unsigned arithmetic to those words. */
void udisk_expand_block_map(const uint32_t allocations[UDISK_ALLOCATION_COUNT],
                            uint8_t map[UDISK_MAP_BYTES])
{
    for (size_t slot = 0; slot < UDISK_ALLOCATION_COUNT; ++slot) {
        uint32_t address = allocations[slot];
        for (uint32_t within_allocation = 0; within_allocation < UDISK_ALLOCATION_BYTES;
             within_allocation += 0x4000) {
            uint32_t entry = address + within_allocation;
            for (unsigned byte = 0; byte < sizeof(entry); ++byte)
                *map++ = (uint8_t)(entry >> (8 * byte));
        }
    }
}

/* ARM9 0x020003D8. The updater calls this on 128 KiB program and readback buffers. */
uint32_t udisk_byte_sum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;
    for (size_t offset = 0; offset < length; ++offset)
        sum += data[offset];
    return sum;
}
