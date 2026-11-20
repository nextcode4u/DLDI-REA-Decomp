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
    got_entry nrio_state_4d2c
    got_entry nrio_state_4d30
    got_entry nrio_state_4d34
    got_entry nrio_write_buffer
    got_entry nrio_state_4d3c
    got_entry nrio_sector_capacity
    got_entry nrio_new_sector_request
    got_entry nrio_physical_block_table
    got_entry nrio_state_4d88
    got_entry nrio_state_4d8c
    got_entry nrio_state_4d90
    got_entry nrio_state_4d94
    got_entry nrio_state_4d98
    got_entry nrio_current_zone
    got_entry nrio_old_physical_block
    got_entry nrio_state_4da4
    got_entry nrio_pending_page_count
    got_entry nrio_state_4de8
    got_entry nrio_free_block_table
    got_entry nrio_nand_data_base
    got_entry nrio_state_4df4
    got_entry nrio_state_4df8
    got_entry nrio_cached_logical_page
    got_entry nrio_pages_per_block
    got_entry nrio_zone_free_cursor
    got_entry nrio_state_4f00
    got_entry nrio_spare
    got_entry nrio_state_5040
    got_entry nrio_current_sector
    got_entry nrio_state_5048
    got_entry nrio_state_504c
    got_entry nrio_state_5050
    got_entry nrio_state_5150
    got_entry nrio_state_5154
    got_entry nrio_state_5158
    got_entry nrio_state_5258
    got_entry nrio_state_525c
    got_entry nrio_sector_base
    got_entry nrio_read_buffer
    got_entry nrio_state_5268
    got_entry nrio_page_buffer
    got_entry nrio_state_5aa8
    got_entry nrio_state_5ae8
    got_entry nrio_logical_block_map
    got_entry nrio_page_in_block_mask
    got_entry nrio_state_5af4
    got_entry nrio_nand_address
    got_entry nrio_state_5af8
    got_entry nrio_state_5afc
    got_entry nrio_state_5b3c
    got_entry nrio_sector_in_page
    got_entry nrio_state_5b44
    got_entry nrio_sector_count
    got_entry nrio_state_5b88
    got_entry nrio_pending_physical_block
    got_entry nrio_state_5b90
    got_entry nrio_pending_logical_block
    got_entry nrio_state_5b98
    got_entry nrio_state_5b9c
    got_entry nrio_block_table_address
