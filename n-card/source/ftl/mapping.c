#include "state.h"

extern void nrio_select_physical_page(u32 block, u32 page);
extern void nrio_page_operation(u32 operation);
extern u8 nrio_check_spare_markers(void);
extern void nrio_clear_block_map(void);
extern void nrio_load_physical_block_table(void);
extern u8 nrio_classify_physical_block(u8 block);
extern u8 nrio_find_free_block(void);

/* Zone zero has a dedicated set of tables. Two other zones share the remaining
 * caches. Preserve each cached zone's pending write before switching tables.
 */
NRIO_ROUTINE(2228) void nrio_load_zone(void)
{
    u8 *table;
    u8 valid_pages;
    u8 classification;
    int spare_status;
    u8 page;
    u32 block;

    if (nrio_initialize_zone_zero != 0) {
        nrio_second_cache_active = 1;
        nrio_physical_block_table = nrio_zone_zero_physical;
        nrio_logical_block_map = nrio_zone_zero_mapping;
        nrio_initialize_zone_zero = 0;
        nrio_nonzero_zone_active = 0;
        nrio_first_cached_zone = 0;
        nrio_pending_logical_block = nrio_zero_pending_logical;
        nrio_pending_physical_block = nrio_zero_pending_physical;
        nrio_old_physical_block = nrio_zero_old_physical;
        nrio_second_cached_zone = 0;
        nrio_pending_page_count = nrio_zero_pending_pages;
        nrio_free_block_table = nrio_zone_zero_free;
        nrio_clear_block_map();
        nrio_pending_logical_block = 0xff;
        nrio_pending_page_count = 0;
        nrio_pending_physical_block = 0xff;
        nrio_old_physical_block = 0xff;
#ifdef NRIO_UNIFIED
        if (!nrio_reload_validated_table()) return;
#else
        nrio_load_physical_block_table();
#endif
        block = 0;
        do {
            table = nrio_free_block_table;
            classification = nrio_classify_physical_block((u8)block);
            table[block] = classification;
            block = block + 1;
        } while (block != 0x40);
        if (nrio_pending_physical_block == 0xff) {
            return;
        }
        nrio_old_physical_block = nrio_logical_block_map[nrio_pending_logical_block];
        if (nrio_pages_per_block == 0) {
            return;
        }
        page = 0;
        do {
            nrio_select_physical_page(nrio_pending_physical_block, page);
            nrio_page_operation(10);
            spare_status = nrio_check_spare_markers();
            valid_pages = page + 1;
            if (spare_status != 0) {
                valid_pages = nrio_pending_page_count;
            }
            nrio_pending_page_count = valid_pages;
            page = page + 1;
        } while (page < nrio_pages_per_block);
        return;
    }
    if (nrio_current_zone == 0) {
        if (nrio_nonzero_zone_active == 0) {
            return;
        }
        if (nrio_second_cache_active == 0) {
            nrio_first_pending_physical = nrio_pending_physical_block;
            nrio_first_old_physical = nrio_old_physical_block;
            nrio_first_pending_logical = nrio_pending_logical_block;
            nrio_first_pending_pages = nrio_pending_page_count;
        } else {
            nrio_second_pending_physical = nrio_pending_physical_block;
            nrio_second_old_physical = nrio_old_physical_block;
            nrio_second_pending_logical = nrio_pending_logical_block;
            nrio_second_pending_pages = nrio_pending_page_count;
        }
        nrio_physical_block_table = nrio_zone_zero_physical;
        nrio_logical_block_map = nrio_zone_zero_mapping;
        nrio_free_block_table = nrio_zone_zero_free;
        nrio_old_physical_block = nrio_zero_old_physical;
        nrio_pending_page_count = nrio_zero_pending_pages;
        nrio_nonzero_zone_active = 0;
        nrio_pending_physical_block = nrio_zero_pending_physical;
        nrio_pending_logical_block = nrio_zero_pending_logical;
        return;
    }
    if (nrio_first_cached_zone == nrio_current_zone) {
        if (nrio_second_cache_active == 0) {
            if (nrio_nonzero_zone_active != 0) {
                return;
            }
        } else if (nrio_nonzero_zone_active != 0) {
            nrio_second_pending_physical = nrio_pending_physical_block;
            nrio_second_old_physical = nrio_old_physical_block;
            nrio_second_pending_logical = nrio_pending_logical_block;
            nrio_second_pending_pages = nrio_pending_page_count;
            goto activate_first_cache;
        }
        nrio_zero_pending_physical = nrio_pending_physical_block;
        nrio_zero_old_physical = nrio_old_physical_block;
        nrio_zero_pending_logical = nrio_pending_logical_block;
        nrio_zero_pending_pages = nrio_pending_page_count;
    activate_first_cache:
        nrio_physical_block_table = nrio_first_cache_physical;
        nrio_logical_block_map = nrio_first_cache_mapping;
        nrio_free_block_table = nrio_first_cache_free;
        nrio_second_cache_active = 0;
        nrio_pending_logical_block = nrio_first_pending_logical;
        nrio_pending_physical_block = nrio_first_pending_physical;
        nrio_nonzero_zone_active = 1;
        nrio_pending_page_count = nrio_first_pending_pages;
        nrio_old_physical_block = nrio_first_old_physical;
        return;
    }
    if (nrio_second_cached_zone == nrio_current_zone) {
        if (nrio_second_cache_active == 0) {
            if (nrio_nonzero_zone_active != 0) {
                nrio_first_pending_physical = nrio_pending_physical_block;
                nrio_first_old_physical = nrio_old_physical_block;
                nrio_first_pending_logical = nrio_pending_logical_block;
                nrio_first_pending_pages = nrio_pending_page_count;
                goto activate_second_cache;
            }
        } else if (nrio_nonzero_zone_active != 0) {
            return;
        }
        nrio_zero_pending_physical = nrio_pending_physical_block;
        nrio_zero_old_physical = nrio_old_physical_block;
        nrio_zero_pending_logical = nrio_pending_logical_block;
        nrio_zero_pending_pages = nrio_pending_page_count;
    activate_second_cache:
        nrio_physical_block_table = nrio_second_cache_physical;
        nrio_logical_block_map = nrio_second_cache_mapping;
        nrio_free_block_table = nrio_second_cache_free;
        nrio_old_physical_block = nrio_second_old_physical;
        nrio_pending_page_count = nrio_second_pending_pages;
        nrio_nonzero_zone_active = 1;
        nrio_pending_physical_block = nrio_second_pending_physical;
        nrio_pending_logical_block = nrio_second_pending_logical;
        nrio_second_cache_active = 1;
        return;
    }
    if (nrio_nonzero_zone_active == 0) {
        nrio_zero_pending_physical = nrio_pending_physical_block;
        nrio_zero_old_physical = nrio_old_physical_block;
        nrio_zero_pending_logical = nrio_pending_logical_block;
        nrio_zero_pending_pages = nrio_pending_page_count;
        if (nrio_second_cache_active == 0) {
        replace_second_cache:
            nrio_physical_block_table = nrio_second_cache_physical;
            nrio_logical_block_map = nrio_second_cache_mapping;
            nrio_free_block_table = nrio_second_cache_free;
            nrio_pending_physical_block = nrio_second_pending_physical;
            nrio_old_physical_block = nrio_second_old_physical;
            nrio_pending_logical_block = nrio_second_pending_logical;
            nrio_pending_page_count = nrio_second_pending_pages;
            nrio_second_cached_zone = nrio_current_zone;
            nrio_second_cache_active = 1;
            goto scan_new_zone;
        }
    } else {
        if (nrio_second_cache_active == 0) {
            nrio_first_pending_physical = nrio_pending_physical_block;
            nrio_first_old_physical = nrio_old_physical_block;
            nrio_first_pending_logical = nrio_pending_logical_block;
            nrio_first_pending_pages = nrio_pending_page_count;
            goto replace_second_cache;
        }
        nrio_second_pending_physical = nrio_pending_physical_block;
        nrio_second_old_physical = nrio_old_physical_block;
        nrio_second_pending_logical = nrio_pending_logical_block;
        nrio_second_pending_pages = nrio_pending_page_count;
    }
    nrio_pending_physical_block = nrio_first_pending_physical;
    nrio_old_physical_block = nrio_first_old_physical;
    nrio_pending_logical_block = nrio_first_pending_logical;
    nrio_pending_page_count = nrio_first_pending_pages;
    nrio_physical_block_table = nrio_first_cache_physical;
    nrio_logical_block_map = nrio_first_cache_mapping;
    nrio_free_block_table = nrio_first_cache_free;
    nrio_second_cache_active = 0;
    nrio_first_cached_zone = nrio_current_zone;
scan_new_zone:
    nrio_clear_block_map();
    nrio_pending_logical_block = 0xff;
    nrio_pending_physical_block = 0xff;
    nrio_old_physical_block = 0xff;
    nrio_pending_page_count = 0;
#ifdef NRIO_UNIFIED
    if (!nrio_reload_validated_table()) return;
#else
    nrio_load_physical_block_table();
#endif
    block = 0;
    do {
        table = nrio_free_block_table;
        classification = nrio_classify_physical_block((u8)block);
        table[block] = classification;
        block = block + 1;
    } while (block != 0x40);
    if ((nrio_pending_physical_block != 0xff) &&
        (nrio_old_physical_block = nrio_logical_block_map[nrio_pending_logical_block],
         nrio_pages_per_block != 0)) {
        page = 0;
        do {
            nrio_select_physical_page(nrio_pending_physical_block, page);
            nrio_page_operation(10);
            spare_status = nrio_check_spare_markers();
            valid_pages = page + 1;
            if (spare_status != 0) {
                valid_pages = nrio_pending_page_count;
            }
            nrio_pending_page_count = valid_pages;
            page = page + 1;
        } while (page < nrio_pages_per_block);
    }
    nrio_nonzero_zone_active = 1;
    return;
}

/* A new request sets the logical address. Sequential reads advance the cursor.
 * The labels preserve the original branch layout under GCC 4.1.1.
 */
NRIO_ROUTINE(3874) u32 nrio_read_sector_with_ecc_mapping(void)
{
    if (nrio_new_sector_request == 0) {
        goto advance;
    }

    u32 sector = nrio_current_sector + nrio_sector_base;
    u32 page = sector >> 2;
    nrio_sector_in_current_page = sector & 3;
    nrio_page_in_current_block = page & nrio_page_in_block_mask;
    nrio_logical_block_index = page >> nrio_sectors_per_block_shift;
    nrio_block_in_current_zone = nrio_logical_block_index % 62;
    nrio_zone_index = nrio_logical_block_index / 62;
    if (nrio_current_zone != nrio_zone_index) {
        nrio_current_zone = nrio_zone_index;
        nrio_load_zone();
    }

check_page:
    if (nrio_sector_in_current_page != 0) {
        goto check_request;
    }

map_page:
    nrio_physical_block_cursor = nrio_logical_block_map[nrio_block_in_current_zone];
    if (nrio_pending_logical_block == nrio_block_in_current_zone) {
        goto pending;
    }
    if (__builtin_expect(nrio_physical_block_cursor == 0xff, 0)) {
        goto use_free_block;
    }
    nrio_select_physical_page(nrio_physical_block_cursor, nrio_page_in_current_block);
load_spare:
    nrio_page_operation(1);
    nrio_check_spare_markers();
read_sector:
    --nrio_sector_count;
    nrio_new_sector_request = 0;
    nrio_sector_in_page = nrio_sector_in_current_page;
    nrio_sector_in_current_page = (nrio_sector_in_current_page + 1) & 3;
    nrio_page_operation(2);
    return 0;

advance:
    if (nrio_sector_in_current_page != 0) {
        goto read_sector;
    }
    nrio_page_in_current_block = (nrio_page_in_current_block + 1) & nrio_page_in_block_mask;
    if (nrio_page_in_current_block == 0) {
        goto advance_block;
    }
    goto map_page;
check_request:
    if (nrio_new_sector_request != 0) {
        goto map_page;
    }
    goto read_sector;

pending:
    if (__builtin_expect(nrio_page_in_current_block < nrio_pending_page_count, 0)) {
        goto new_block;
    }
    if (__builtin_expect(nrio_old_physical_block == 0xff, 0)) {
        goto use_free_block;
    }
    nrio_select_physical_page(nrio_old_physical_block, nrio_page_in_current_block);
    goto load_spare;
advance_block:
    nrio_block_in_current_zone = (u8)(nrio_block_in_current_zone + 1) % 62;
    if (__builtin_expect(nrio_block_in_current_zone != 0, 0)) {
        goto map_page;
    }
    nrio_current_zone++;
    nrio_load_zone();
    goto check_page;
use_free_block:
    nrio_select_physical_page(nrio_find_free_block(), 0);
    goto load_spare;
new_block:
    nrio_select_physical_page(nrio_pending_physical_block, nrio_page_in_current_block);
    goto load_spare;
}
