#include "../ftl/state.h"

extern u8 nrio_read_status(void);
extern void nrio_page_operation(u32 operation);

NRIO_ROUTINE(1540) void nrio_clear_block_map(void)
{
    u32 index;
    for (index = 0; index != 62; ++index) {
        nrio_logical_block_map[index] = 0xff;
    }
}

NRIO_ROUTINE(1580) void nrio_clear_free_table(void)
{
    u32 index;
    for (index = 0; index != 64; ++index) {
        nrio_free_block_table[index] = 0xff;
    }
}

NRIO_ROUTINE(15c0) void nrio_set_nand_address(u32 address)
{
    nrio_nand_address = address;
}

NRIO_ROUTINE(15e8) void nrio_select_physical_page(u32 block, u32 page)
{
    nrio_nand_address = nrio_nand_data_base +
        ((nrio_physical_block_table[block] * nrio_pages_per_block + page) << 11);
}

NRIO_ROUTINE(1814) u8 nrio_find_free_block(void)
{
    u8 index = nrio_zone_free_cursor[nrio_current_zone];
    do {
        if (nrio_free_block_table[index] == 0xff) {
            return index;
        }
        ++index;
        if (index >= 64) {
            index = 0;
        }
    } while (index != nrio_zone_free_cursor[nrio_current_zone]);
    return 0;
}

NRIO_ROUTINE(191c) unsigned nrio_decode_nand_status(void)
{
    u8 status = nrio_read_status();
    if ((status & 0x40) == 0) {
        return 2;
    }
    if (status & 0x80) {
        return (status & 1) ? 2 : 0;
    }
    return 3;
}

NRIO_ROUTINE(2004) u8 nrio_allocate_free_block(void)
{
    u8 block = nrio_zone_free_cursor[nrio_current_zone];

    do {
        if (nrio_free_block_table[block] == 0xff) {
            nrio_select_physical_page(block, 0);
            nrio_page_operation(7);
            u32 status = nrio_decode_nand_status();
            if (status == 0) {
                nrio_free_block_table[block] = status;
                nrio_zone_free_cursor[nrio_current_zone] = block;
                return block;
            }
            nrio_free_block_table[block] = 0xfe;
        }
        ++block;
        if (block >= 64) {
            block = 0;
        }
    } while (nrio_zone_free_cursor[nrio_current_zone] != block);

    return 0xff;
}
