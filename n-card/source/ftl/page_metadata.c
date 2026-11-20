#include "state.h"

extern void nrio_select_physical_page(u32 block, u32 page);
extern unsigned nrio_decode_nand_status(void);

extern void nrio_page_operation(u32 operation);

NRIO_ROUTINE(164c) void nrio_set_spare_markers(void)
{
    u8 *spare = nrio_spare;
    u8 marker = spare[11];
    ((volatile u8 *)spare)[7] = marker;
    ((volatile u8 *)spare)[2] = 0x55;
    ((volatile u8 *)spare)[3] = 0xaa;
    ((volatile u8 *)spare)[0] = 0x55;
    ((volatile u8 *)spare)[1] = 0xaa;
    ((volatile u8 *)spare)[15] = marker;
}

NRIO_ROUTINE(1e68) unsigned nrio_write_page_marker(u32 page)
{
    nrio_spare[11] = nrio_pending_logical_block;
    nrio_select_physical_page(nrio_pending_physical_block, page);
    nrio_set_spare_markers();
    nrio_page_operation(8);
    return nrio_decode_nand_status();
}

NRIO_ROUTINE(20c8) u32 nrio_retry_block_erase(u32 block)
{
    u8 attempts = 2;
    u32 status;
    do {
        nrio_select_physical_page(block, 0);
        nrio_page_operation(7);
        status = nrio_decode_nand_status();
        --attempts;
        if (status == 0) {
            break;
        }
    } while (attempts != 0xff);
    return status;
}
