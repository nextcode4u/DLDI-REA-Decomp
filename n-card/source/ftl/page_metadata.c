#include "state.h"

extern void nrio_set_nand_address(u32 address);
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

static inline void repair_block_marker(void)
{
    if (nrio_spare[11] != nrio_spare[15] && nrio_spare[7] == nrio_spare[15]) {
        nrio_spare[11] = nrio_spare[7];
    }
}

NRIO_ROUTINE(1694) u8 nrio_check_spare_markers(void)
{
    if (nrio_spare[0] == 0xff && nrio_spare[2] == 0xff) {
        repair_block_marker();
        if (nrio_spare[11] != 0xff) {
            return 0xfe;
        }
        return 0xff;
    }
    if (nrio_spare[0] != 0x55 && nrio_spare[2] != 0x55) {
        return 0xfe;
    }
    if (nrio_spare[1] != 0xaa && nrio_spare[3] != 0xaa) {
        return 0xfe;
    }
    repair_block_marker();
    if (nrio_spare[11] == 0xff) {
        return 0xfe;
    }

    for (u8 offset = 0; offset != 64; offset += 16) {
        if (nrio_spare[offset | 8] != nrio_spare[offset | 12] &&
            nrio_spare[offset | 4] == nrio_spare[offset | 12]) {
            nrio_spare[offset | 8] = nrio_spare[offset | 4];
        }
        if (nrio_spare[offset | 9] != nrio_spare[offset | 13] &&
            nrio_spare[offset | 5] == nrio_spare[offset | 13]) {
            nrio_spare[offset | 9] = nrio_spare[offset | 5];
        }
        if (nrio_spare[offset | 10] != nrio_spare[offset | 14] &&
            nrio_spare[offset | 6] == nrio_spare[offset | 14]) {
            nrio_spare[offset | 10] = nrio_spare[offset | 6];
        }
    }
    return 0;
}

NRIO_ROUTINE(1cb8) void nrio_load_physical_block_table(void)
{
    nrio_set_nand_address(nrio_block_table_address + ((nrio_current_zone >> 4) << 11));
    nrio_page_operation(1);
    if (nrio_check_spare_markers() == 0 && nrio_spare[11] == 0xab) {
        nrio_sector_in_page = (nrio_current_zone & 15) >> 2;
        nrio_page_operation(3);

        u32 *blocks = nrio_physical_block_table;
        const u16 *encoded = nrio_page_buffer + (nrio_current_zone & 15) * 64;
        unsigned i = 0;
        do {
            u16 entry = encoded[i];
            u32 offset = (u32)(u16)((entry >> 8) | (entry << 8)) << 16;
            blocks[i] = (offset - nrio_nand_data_base) / (nrio_pages_per_block << 11);
        } while (++i != 64);
    }
}

NRIO_ROUTINE(2114) u8 nrio_classify_physical_block(u8 block)
{
    u8 retry = 2;
    do {
        nrio_select_physical_page(block, 0);
        nrio_page_operation(10);
        if (nrio_check_spare_markers() == 0xfe) {
            continue;
        }
        u8 first_marker = nrio_spare[11];
        nrio_select_physical_page(block, nrio_page_in_block_mask);
        nrio_page_operation(10);
        if (nrio_check_spare_markers() == 0xfe) {
            continue;
        }
        u8 last_marker = nrio_spare[11];
        if (first_marker == last_marker) {
            if (first_marker != 0xff) {
                nrio_logical_block_map[first_marker] = block;
                return 0;
            }
        } else {
            if (last_marker != 0xff) {
                return 0xfe;
            }
            if (nrio_pending_physical_block == 0xff) {
                nrio_pending_logical_block = first_marker;
                nrio_pending_physical_block = block;
                return 0xfa;
            }
            if (nrio_retry_block_erase(block) != 0) {
                return 0xfe;
            }
        }
        return 0xff;
    } while (--retry != 0xff);
    return 0xfe;
}
