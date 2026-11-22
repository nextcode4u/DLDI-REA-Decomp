#ifndef NRIO_UDISK_UPDATER_H
#define NRIO_UDISK_UPDATER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    UDISK_ALLOCATION_BYTES = 0x20000,
    UDISK_ALLOCATION_COUNT = 0x800,
    UDISK_MAP_BYTES = 0x10000,
    UDISK_MAX_ERASE_FRAMES = 4,
};

bool udisk_update_address_allowed(uint32_t address, uint32_t erase_block_bytes);
size_t udisk_erase_frames(uint32_t address, uint32_t erase_block_bytes, bool send_bank,
                         uint32_t frames[UDISK_MAX_ERASE_FRAMES]);
void udisk_expand_block_map(const uint32_t allocations[UDISK_ALLOCATION_COUNT],
                            uint8_t map[UDISK_MAP_BYTES]);
uint32_t udisk_byte_sum(const uint8_t *data, size_t length);

#endif
