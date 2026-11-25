#include "state.h"

extern void nrio_load_zone(void);
extern u32 nrio_copy_nand_pages(u8 source, u8 destination, u8 first_page, u8 end);
extern u32 nrio_replace_pending_block(void);
extern u32 nrio_flush_block(void);
extern u8 nrio_allocate_free_block(void);
extern u32 nrio_write_page_marker(u32 page);
extern void nrio_select_physical_page(u32 block, u32 page);
extern void nrio_page_operation(u32 operation);
extern u8 nrio_check_spare_markers(void);
extern void nrio_set_spare_markers(void);
extern u32 nrio_decode_nand_status(void);
extern u32 nrio_release_block(u8 block);
extern void nrio_commit_pending_block(void);

NRIO_ROUTINE(3fc0) int nrio_write_sector_with_mapping(void)
{
    u8 read_cursor;
    bool block_replaced;
    int status;
    u8 retries_remaining;
    u32 value;

    if (nrio_new_sector_request == 0) {
        if (((nrio_sector_in_current_page == 0) &&
             (nrio_page_in_current_block =
                  (nrio_page_in_current_block + 1) & nrio_page_in_block_mask,
              nrio_page_in_current_block == 0)) &&
            (value = ((nrio_block_in_current_zone + 1) & 0xff) % 0x3e,
             nrio_block_in_current_zone = (u8)value, value == 0)) {
            nrio_current_zone = nrio_current_zone + 1;
            nrio_load_zone();
#ifdef NRIO_UNIFIED
            if (!nrio_sector_capacity) return 9;
#endif
            goto zone_ready;
        }
    continue_request:
        if (nrio_sector_in_current_page == 0) {
            if (nrio_block_in_current_zone == nrio_pending_logical_block)
                goto reuse_pending_block;
            goto allocate_pending_block;
        }
        value = (u32)(u8)(4 - nrio_sector_in_current_page);
    } else {
        value = (u32)(nrio_current_sector + nrio_sector_base) >> 2;
        nrio_logical_block_index = value >> (s8)nrio_sectors_per_block_shift;
        nrio_sector_in_current_page = (u8)(nrio_current_sector + nrio_sector_base) & 3;
        nrio_page_in_current_block = (u8)value & nrio_page_in_block_mask;
        nrio_block_in_current_zone = (u8)(nrio_logical_block_index % 0x3e);
        nrio_zone_index = (u16)(nrio_logical_block_index / 0x3e);
        if ((u32)nrio_current_zone != (nrio_logical_block_index / 0x3e & 0xffff)) {
            nrio_current_zone = nrio_zone_index;
            nrio_load_zone();
#ifdef NRIO_UNIFIED
            if (!nrio_sector_capacity) return 9;
#endif
        zone_ready:
            if (nrio_new_sector_request == 0)
                goto continue_request;
        }
        if (nrio_block_in_current_zone == nrio_pending_logical_block) {
        reuse_pending_block:
            if (nrio_page_in_current_block < nrio_pending_page_count)
                goto allocate_pending_block;
            if (nrio_old_physical_block == 0xff)
                goto prepare_first_page;
        copy_old_pages:
            retries_remaining = 2;
            do {
                status = nrio_copy_nand_pages(nrio_old_physical_block, nrio_pending_physical_block,
                                              nrio_pending_page_count, nrio_page_in_current_block);
                if (status == 0) {
                    nrio_pending_page_count = nrio_page_in_current_block;
                    break;
                }
                status = nrio_replace_pending_block();
                --retries_remaining;
                if (status != 0) {
                    return status;
                }
            } while (retries_remaining != 0xff);
        } else {
        allocate_pending_block:
            status = nrio_flush_block();
            if (status != 0) {
                return status;
            }
            status = nrio_allocate_free_block();
            nrio_pending_physical_block = (u8)status;
            if (status == 0xff) {
                return 9;
            }
            nrio_free_block_table[status] = 0xfa;
            nrio_pending_logical_block = nrio_block_in_current_zone;
            nrio_old_physical_block = nrio_logical_block_map[nrio_block_in_current_zone];
            if (nrio_old_physical_block != 0xff)
                goto copy_old_pages;
        prepare_first_page:
            if ((nrio_pending_page_count != 0) || (nrio_page_in_current_block == 0))
                goto copy_old_pages;
            retries_remaining = 2;
            do {
                status = nrio_write_page_marker(0);
                if (status == 0) {
                    nrio_pending_page_count = nrio_page_in_current_block;
                    break;
                }
                status = nrio_replace_pending_block();
                --retries_remaining;
                if (status != 0) {
                    return status;
                }
            } while (retries_remaining != 0xff);
        }
        value = (u32)(u8)(4 - nrio_sector_in_current_page);
        if (nrio_sector_in_current_page == 0) {
            if ((nrio_sector_count < 4) && (nrio_old_physical_block != 0xff)) {
                nrio_select_physical_page(nrio_old_physical_block, nrio_page_in_current_block);
                nrio_page_operation(1);
                status = nrio_check_spare_markers();
                if (status == 0) {
                    for (nrio_sector_in_page = nrio_sector_count; nrio_sector_in_page < 4;
                         nrio_sector_in_page = nrio_sector_in_page + 1) {
                        nrio_page_operation(3);
                    }
                }
            }
            nrio_spare[0xb] = nrio_pending_logical_block;
            nrio_select_physical_page(nrio_pending_physical_block, nrio_page_in_current_block);
            goto write_sector;
        }
    }
    if (nrio_new_sector_request != 0) {
        if (nrio_old_physical_block == 0xff) {
            nrio_spare[0xb] = nrio_pending_logical_block;
            nrio_select_physical_page(nrio_pending_physical_block, nrio_page_in_current_block);
            nrio_sector_in_page = 0;
            if (nrio_sector_in_current_page != 0) {
                do {
                    nrio_page_operation(9);
                    nrio_sector_in_page = nrio_sector_in_page + 1;
                } while (nrio_sector_in_page < nrio_sector_in_current_page);
            }
        } else {
            nrio_select_physical_page(nrio_old_physical_block, nrio_page_in_current_block);
            nrio_page_operation(1);
            status = nrio_check_spare_markers();
            if (status == 0) {
                nrio_sector_in_page = 0;
                read_cursor = 0;
                if (nrio_sector_in_current_page != 0) {
                    do {
                        nrio_page_operation(3);
                        nrio_sector_in_page = nrio_sector_in_page + 1;
                        read_cursor = nrio_sector_in_current_page;
                    } while (nrio_sector_in_page < nrio_sector_in_current_page);
                }
                if (nrio_sector_count < value) {
                    while (nrio_sector_in_page = read_cursor + 1, nrio_sector_in_page < 4) {
                        nrio_page_operation(3);
                        read_cursor = nrio_sector_in_page;
                    }
                }
                nrio_spare[0xb] = nrio_pending_logical_block;
                nrio_select_physical_page(nrio_pending_physical_block, nrio_page_in_current_block);
                nrio_sector_in_page = 0;
                if (nrio_sector_in_current_page != 0) {
                    do {
                        nrio_page_operation(6);
                        nrio_sector_in_page = nrio_sector_in_page + 1;
                    } while (nrio_sector_in_page < nrio_sector_in_current_page);
                }
            } else {
                nrio_spare[0xb] = nrio_pending_logical_block;
                nrio_select_physical_page(nrio_pending_physical_block, nrio_page_in_current_block);
                nrio_sector_in_page = 0;
                if (nrio_sector_in_current_page != 0) {
                    do {
                        nrio_page_operation(9);
                        nrio_sector_in_page = nrio_sector_in_page + 1;
                    } while (nrio_sector_in_page < nrio_sector_in_current_page);
                }
            }
        }
    }
write_sector:
    nrio_sector_in_page = nrio_sector_in_current_page;
    nrio_page_operation(5);
    if ((nrio_sector_count == 1) && (1 < value)) {
        nrio_sector_count = 1;
        status = 1;
        nrio_sector_in_page = nrio_sector_in_current_page;
        if (nrio_old_physical_block == 0xff) {
            while (nrio_sector_in_page = nrio_sector_in_page + 1, nrio_sector_in_page < 4) {
                nrio_page_operation(9);
            }
        } else {
            while (nrio_sector_count = status, nrio_sector_in_page = nrio_sector_in_page + 1,
                   nrio_sector_in_page < 4) {
                nrio_page_operation(6);
                status = nrio_sector_count;
            }
        }
        nrio_set_spare_markers();
        nrio_page_operation(4);
        status = nrio_decode_nand_status();
        if ((status == 0) || (status = nrio_replace_pending_block(), status == 0)) {
            nrio_pending_page_count = nrio_pending_page_count + 1;
            if (nrio_pages_per_block == nrio_pending_page_count) {
                if (nrio_old_physical_block != 0xff) {
                    nrio_release_block(nrio_old_physical_block);
                }
                nrio_commit_pending_block();
            }
            nrio_sector_count = nrio_sector_count + -1;
            nrio_sector_in_current_page = (nrio_sector_in_current_page + 1) & 3;
            status = 0;
            nrio_new_sector_request = 0;
        }
    } else if (nrio_sector_in_current_page == 3) {
        nrio_set_spare_markers();
        nrio_page_operation(4);
        status = nrio_decode_nand_status();
        if (status == 0) {
            block_replaced = false;
        } else {
            status = nrio_replace_pending_block();
            if (status != 0) {
                return status;
            }
            block_replaced = true;
        }
        nrio_pending_page_count = nrio_pending_page_count + 1;
        if (nrio_pages_per_block == nrio_pending_page_count) {
            if (nrio_old_physical_block != 0xff) {
                nrio_release_block(nrio_old_physical_block);
            }
            nrio_commit_pending_block();
        }
        nrio_sector_count = nrio_sector_count - 1;
        nrio_sector_in_current_page = (nrio_sector_in_current_page + 1) & 3;
        if (block_replaced) {
            status = 0x10;
        } else {
            status = 0;
        }
        nrio_new_sector_request = 0;
    } else {
        nrio_sector_count = nrio_sector_count - 1;
        nrio_sector_in_current_page = (nrio_sector_in_current_page + 1) & 3;
        nrio_new_sector_request = 0;
        status = 0;
    }
    return status;
}
