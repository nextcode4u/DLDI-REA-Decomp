#include "state.h"

extern void nrio_select_physical_page(u32 block, u32 page);
extern void nrio_page_operation(u32 operation);
extern u8 nrio_find_free_block(void);
extern void nrio_load_zone(void);

NRIO_ROUTINE(2f58) void nrio_read_sector_with_mapping(void)
{
    u32 sector = nrio_current_sector + nrio_sector_base;
    if (nrio_current_sector >> 2 != nrio_cached_logical_page) {
        u32 page = sector >> 2;
        nrio_cached_logical_page = nrio_current_sector >> 2;
        u8 page_in_block = page & nrio_page_in_block_mask;
        u32 block = page / nrio_pages_per_block;
        u16 zone = block / 62;
        u8 block_in_zone = block % 62;
        if (nrio_current_zone != zone) {
            nrio_current_zone = zone;
            nrio_load_zone();
        }

        u8 physical = nrio_logical_block_map[block_in_zone];
        if (nrio_pending_logical_block == block_in_zone) {
            if (page_in_block >= nrio_pending_page_count) {
                physical = nrio_old_physical_block;
            } else {
                nrio_select_physical_page(nrio_pending_physical_block, page_in_block);
                goto read_page;
            }
        }
        if (physical == 0xff) {
            nrio_select_physical_page(nrio_find_free_block(), 0);
        } else {
            nrio_select_physical_page(physical, page_in_block);
        }
read_page:
        nrio_page_operation(1);
    }
    nrio_sector_in_page = sector & 3;
    nrio_page_operation(11);
}
