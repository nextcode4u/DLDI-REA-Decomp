#include "state.h"

extern u32 nrio_retry_block_erase(u32 block);
extern u32 nrio_copy_nand_pages(u8 source, u8 destination, u8 first_page, u8 end_page);
extern unsigned nrio_write_page_marker(u32 page);
extern u8 nrio_allocate_free_block(void);
extern u8 nrio_find_free_block(void);
extern void nrio_load_zone(void);

NRIO_ROUTINE(1888) void nrio_commit_pending_block(void)
{
    nrio_free_block_table[nrio_pending_physical_block] = 0;
    nrio_logical_block_map[nrio_pending_logical_block] = nrio_pending_physical_block;
    nrio_pending_physical_block = 0xff;
    nrio_old_physical_block = 0xff;
    nrio_pending_logical_block = 0xff;
    nrio_pending_page_count = 0;
}

NRIO_ROUTINE(3bb8) u32 nrio_release_block(u8 block)
{
    if (block == 0xff) {
        return 0;
    }

    if (nrio_retry_block_erase(block) == 0) {
        nrio_free_block_table[block] = 0xff;
        return 0;
    }

    nrio_free_block_table[block] = 0xfe;
    return 0xfe;
}

NRIO_ROUTINE(3c30) u32 nrio_replace_pending_block(void)
{
    u8 remaining = 2;
    u32 result;

    do {
        if (nrio_pending_physical_block == 0xff || nrio_pending_page_count == 0) {
            result = 0;
            break;
        }

        u8 replacement = nrio_allocate_free_block();
        if (replacement == 0xff) {
            result = 9;
            break;
        }

        u32 status = nrio_copy_nand_pages(nrio_pending_physical_block, replacement, 0,
                                          nrio_pending_page_count);
        if (status == 0) {
            nrio_release_block(nrio_pending_physical_block);
            nrio_free_block_table[nrio_pending_physical_block] = 0xfe;
            result = status;
            nrio_free_block_table[replacement] = 0xfa;
            nrio_pending_physical_block = replacement;
            break;
        }

        nrio_free_block_table[replacement] = 0xfe;
        nrio_release_block(replacement);
        result = status;
    } while (--remaining != 0xff);

    return result;
}

NRIO_ROUTINE(3d28) u32 nrio_flush_block(void)
{
    u8 remaining = 2;
    u32 status;

    if (nrio_pending_physical_block == 0xff) {
        return 0;
    }
    if (nrio_old_physical_block == 0xff) {
        if (nrio_pending_page_count == nrio_pages_per_block) {
            nrio_commit_pending_block();
            return 0;
        }
        do {
            if (nrio_write_page_marker(nrio_page_in_block_mask) == 0) {
                nrio_commit_pending_block();
                return 0;
            }
            status = nrio_replace_pending_block();
            --remaining;
            if (status != 0) {
                return status;
            }
        } while (remaining != 0xff);
    } else {
        do {
            if (nrio_copy_nand_pages(nrio_old_physical_block, nrio_pending_physical_block, nrio_pending_page_count,
                           nrio_pages_per_block) == 0) {
                nrio_release_block(nrio_old_physical_block);
                nrio_commit_pending_block();
                return 0;
            }
            status = nrio_replace_pending_block();
            --remaining;
            if (status != 0) {
                return status;
            }
        } while (remaining != 0xff);
    }
    return 2;
}

NRIO_ROUTINE(3e5c) u32 nrio_sector_nand_address(u32 offset)
{
    u32 sector = offset >> 9;
    nrio_current_sector = sector;
    if (sector + 1 > nrio_sector_capacity) {
        return 0;
    }

    sector += nrio_sector_base;
    u32 page = sector >> 2;
    u8 page_in_block_mask = nrio_page_in_block_mask;
    u32 block = page / nrio_pages_per_block;
    u16 zone = block / 62;
    u8 block_in_zone = block % 62;
    if (zone != nrio_current_zone) {
        nrio_current_zone = zone;
        nrio_load_zone();
#ifdef NRIO_UNIFIED
        if (!nrio_sector_capacity) return 0;
#endif
    }

    if (nrio_pending_logical_block == block_in_zone && nrio_flush_block() != 0) {
        return 0;
    }
    u8 physical = nrio_logical_block_map[block_in_zone];
    if (physical == 0xff) {
        physical = nrio_find_free_block();
    }
    u8 page_in_block = (sector >> 2) & page_in_block_mask;
    u32 physical_page = nrio_physical_block_table[physical] * nrio_pages_per_block +
                        page_in_block;
    return nrio_nand_data_base + ((physical_page * 4 + (sector & 3)) << 9);
}
