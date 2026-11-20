.section .nrio.strings, "a", %progbits
.global mftl_sectors
mftl_sectors:
    .asciz "mftl_sectors"
    .balign 4, 0
.global mftl_spare
mftl_spare:
    .asciz "mftl_spare"
    .balign 4, 0
.global mftl_blocktab
mftl_blocktab:
    .asciz "mftl_blocktab"
    .balign 4, 0
.global mftl_freetab
mftl_freetab:
    .asciz "mftl_freetab"
    .balign 4, 0
.global mftl_freeptr
mftl_freeptr:
    .asciz "mftl_freeptr"
    .balign 4, 0
.section .nrio.got, "aw", %progbits
.global nrio_got
.macro got_entry symbol
    .global __got_offset_\symbol
    .set __got_offset_\symbol, . - nrio_got
    .word \symbol
.endm
nrio_got:
    .word   0x00000000
    .word   0x00000000
    .word   0x00000000
    got_entry nrio_block_count
    got_entry nrio_zero_pending_logical
    got_entry nrio_physical_block_cursor
    got_entry nrio_write_buffer
    got_entry nrio_first_cache_free
    got_entry nrio_sector_capacity
    got_entry nrio_new_sector_request
    got_entry nrio_physical_block_table
    got_entry nrio_logical_block_index
    got_entry nrio_second_pending_logical
    got_entry nrio_first_cached_zone
    got_entry nrio_first_pending_logical
    got_entry nrio_zone_count
    got_entry nrio_current_zone
    got_entry nrio_old_physical_block
    got_entry nrio_second_cache_mapping
    got_entry nrio_pending_page_count
    got_entry nrio_zero_pending_pages
    got_entry nrio_free_block_table
    got_entry nrio_nand_data_base
    got_entry nrio_first_old_physical
    got_entry nrio_initialize_zone_zero
    got_entry nrio_cached_logical_page
    got_entry nrio_pages_per_block
    got_entry nrio_zone_free_cursor
    got_entry nrio_first_cache_physical
    got_entry nrio_spare
    got_entry nrio_nonzero_zone_active
    got_entry nrio_current_sector
    got_entry nrio_sector_in_current_page
    got_entry nrio_zone_index
    got_entry nrio_zone_zero_physical
    got_entry nrio_second_pending_pages
    got_entry nrio_zero_pending_physical
    got_entry nrio_second_cache_physical
    got_entry nrio_second_old_physical
    got_entry nrio_block_in_current_zone
    got_entry nrio_sector_base
    got_entry nrio_read_buffer
    got_entry nrio_second_cache_free
    got_entry nrio_page_buffer
    got_entry nrio_first_cache_mapping
    got_entry nrio_page_in_current_block
    got_entry nrio_logical_block_map
    got_entry nrio_page_in_block_mask
    got_entry nrio_second_cached_zone
    got_entry nrio_nand_address
    got_entry nrio_second_pending_physical
    got_entry nrio_zone_zero_mapping
    got_entry nrio_zero_old_physical
    got_entry nrio_sector_in_page
    got_entry nrio_zone_zero_free
    got_entry nrio_sector_count
    got_entry nrio_sectors_per_block_shift
    got_entry nrio_pending_physical_block
    got_entry nrio_first_pending_pages
    got_entry nrio_pending_logical_block
    got_entry nrio_second_cache_active
    got_entry nrio_first_pending_physical
    got_entry nrio_block_table_address
