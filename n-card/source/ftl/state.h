#ifndef NRIO_STATE_H
#define NRIO_STATE_H

#include "../nrio.h"

extern u32 nrio_nand_address;
extern u32 nrio_nand_data_base;
extern u32 *nrio_physical_block_table;
extern u16 nrio_current_zone;
extern u8 nrio_pages_per_block;
extern u8 nrio_page_in_block_mask;
extern u8 *nrio_logical_block_map;
extern u8 *nrio_free_block_table;
extern u8 nrio_zone_free_cursor[256];
extern u8 nrio_spare[64];
extern u8 nrio_pending_logical_block;
extern u8 nrio_pending_physical_block;
extern u8 nrio_old_physical_block;
extern u8 nrio_pending_page_count;

#endif
