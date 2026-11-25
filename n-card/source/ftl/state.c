#include "state.h"

#define STATE(name) __attribute__((section(".nrio_bss." #name), aligned(4)))

u32 nrio_nand_address STATE(4d28);
u16 nrio_block_count STATE(4d2c);
u8 nrio_zero_pending_logical STATE(4d30);
u8 nrio_physical_block_cursor STATE(4d34);
const u8 *nrio_write_buffer STATE(4d38);
u8 nrio_first_cache_free[64] STATE(4d3c);
u32 nrio_sector_capacity STATE(4d7c);
u8 nrio_new_sector_request STATE(4d80);
u32 *nrio_physical_block_table STATE(4d84);
u32 nrio_logical_block_index STATE(4d88);
u8 nrio_second_pending_logical STATE(4d8c);
u16 nrio_first_cached_zone STATE(4d90);
u8 nrio_first_pending_logical STATE(4d94);
u16 nrio_zone_count STATE(4d98);
u16 nrio_current_zone STATE(4d9c);
u8 nrio_old_physical_block STATE(4da0);
u8 nrio_second_cache_mapping[64] STATE(4da4);
u8 nrio_pending_page_count STATE(4de4);
u8 nrio_zero_pending_pages STATE(4de8);
u8 *nrio_free_block_table STATE(4dec);
u32 nrio_nand_data_base STATE(4df0);
u8 nrio_first_old_physical STATE(4df4);
u8 nrio_initialize_zone_zero STATE(4df8);
u8 nrio_pages_per_block STATE(4dfc);
u8 nrio_zone_free_cursor[0x100] STATE(4e00);
u32 nrio_first_cache_physical[64] STATE(4f00);
u8 nrio_spare[0x40] STATE(5000);
u8 nrio_nonzero_zone_active STATE(5040);
u32 nrio_current_sector STATE(5044);
u8 nrio_sector_in_current_page STATE(5048);
u16 nrio_zone_index STATE(504c);
u32 nrio_zone_zero_physical[64] STATE(5050);
u8 nrio_second_pending_pages STATE(5150);
u8 nrio_zero_pending_physical STATE(5154);
u32 nrio_second_cache_physical[64] STATE(5158);
u8 nrio_second_old_physical STATE(5258);
u8 nrio_block_in_current_zone STATE(525c);
u32 nrio_sector_base STATE(5260);
u8 *nrio_read_buffer STATE(5264);
u8 nrio_second_cache_free[64] STATE(5268);
u16 nrio_page_buffer[1024] STATE(52a8);
u8 nrio_first_cache_mapping[64] STATE(5aa8);
u8 nrio_page_in_current_block STATE(5ae8);
u8 *nrio_logical_block_map STATE(5aec);
u8 nrio_page_in_block_mask STATE(5af0);
u16 nrio_second_cached_zone STATE(5af4);
u8 nrio_second_pending_physical STATE(5af8);
u8 nrio_zone_zero_mapping[64] STATE(5afc);
u8 nrio_zero_old_physical STATE(5b3c);
u8 nrio_sector_in_page STATE(5b40);
u8 nrio_zone_zero_free[64] STATE(5b44);
u32 nrio_sector_count STATE(5b84);
u8 nrio_sectors_per_block_shift STATE(5b88);
u8 nrio_pending_physical_block STATE(5b8c);
u8 nrio_first_pending_pages STATE(5b90);
u8 nrio_pending_logical_block STATE(5b94);
u8 nrio_second_cache_active STATE(5b98);
u8 nrio_first_pending_physical STATE(5b9c);
u32 nrio_block_table_address STATE(5ba0);

u32 nrio_cached_logical_page = 0xffffffff;

#ifdef NRIO_UNIFIED
u32 nrio_zone_map_fingerprints[256];
u32 nrio_bus_fault;
#endif
