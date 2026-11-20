#include "state.h"

extern void nrio_page_operation(u32 operation);
extern u8 nrio_check_spare_markers(void);
extern void nrio_set_nand_address(u32 address);

/* Keep the search, stride, and success paths in the original GCC layout. */
NRIO_ROUTINE(1dc4) u32 nrio_find_block_table(void)
{
    u32 address = nrio_nand_data_base - 0x100000;

    goto check_address;

read_spare:
    nrio_set_nand_address(address);
    nrio_page_operation(10);
    if (nrio_check_spare_markers() != 0 || nrio_spare[11] != 0xab) {
        goto next_block;
    }
    goto found;

check_address:
    if (address != nrio_nand_data_base) {
        goto read_spare;
    }
    return false;

next_block:
    address += (u32)nrio_pages_per_block << 11;
    goto check_address;

found:
    nrio_block_table_address = address;
    return true;
}

#ifdef NRIO_C_ONLY
#define LOCAL __attribute__((visibility("hidden")))

extern const char mftl_sectors[] LOCAL, mftl_spare[] LOCAL, mftl_blocktab[] LOCAL;
extern const char mftl_freetab[] LOCAL, mftl_freeptr[] LOCAL;

extern void nrio_require_word_alignment(u32 address, const char *message);
extern void nrio_load_physical_block_table(void);
extern void nrio_clear_block_map(void);
extern void nrio_clear_free_table(void);
extern void nrio_load_zone(void);

NRIO_ROUTINE(31a8) int nrio_init_nand_geometry(u32 block_size, u16 blocks, u32 data_base, u32 mode)
{
    (void)mode;
    nrio_require_word_alignment((u32)nrio_page_buffer, mftl_sectors);
    nrio_require_word_alignment((u32)nrio_spare, mftl_spare);
    nrio_require_word_alignment((u32)nrio_logical_block_map, mftl_blocktab);
    nrio_require_word_alignment((u32)nrio_free_block_table, mftl_freetab);
    nrio_require_word_alignment((u32)nrio_zone_free_cursor, mftl_freeptr);

    u8 pages_per_block = block_size >> 11;
    u8 pages_per_block_shift = (pages_per_block >> 6) + 5;
    nrio_sector_base = 62 << pages_per_block_shift;
    nrio_pages_per_block = pages_per_block;
    nrio_page_in_block_mask = pages_per_block - 1;
    nrio_zone_count = blocks >> 6;
    nrio_sectors_per_block_shift = pages_per_block_shift;
    nrio_block_count = blocks;
    nrio_nand_data_base = data_base;

    for (u8 *p = nrio_zone_free_cursor; p != nrio_zone_free_cursor + 256; ++p) {
        *p = 0;
    }
    nrio_physical_block_table = nrio_zone_zero_physical;
    if (!nrio_find_block_table()) {
        for (;;) {
        }
    }
    nrio_current_zone = 511;
    nrio_load_physical_block_table();

    u16 physical_blocks = nrio_physical_block_table[63];
    u16 zones = physical_blocks >> 6;
    nrio_sector_capacity =
        ((u32)(zones * 62) << (nrio_sectors_per_block_shift + 2)) - nrio_sector_base;
    nrio_block_count = physical_blocks;
    nrio_initialize_zone_zero = 1;
    nrio_logical_block_map = nrio_zone_zero_mapping;
    nrio_free_block_table = nrio_zone_zero_free;
    nrio_pending_physical_block = nrio_zero_pending_physical;
    nrio_old_physical_block = nrio_zero_old_physical;
    nrio_physical_block_table = nrio_zone_zero_physical;
    nrio_current_zone = 0;
    nrio_pending_logical_block = nrio_zero_pending_logical;
    nrio_zone_count = zones;
    nrio_pending_page_count = nrio_zero_pending_pages;

    nrio_clear_block_map();
    nrio_clear_free_table();
    nrio_load_zone();
    return 0;
}
#endif
