#include "../ftl/state.h"

extern void nrio_read_nand_spare(u32 *output);
extern void nrio_read_nand_sector(u32 *output, u32 sector);
extern void nrio_write_nand_sector_raw(const u8 *input, u32 sector);
extern void nrio_read_nand_sector_with_ecc(u8 *output, u32 sector, u8 *ecc);
extern void nrio_write_nand_sector_with_ecc(const u8 *input, u32 sector, u8 *ecc);
extern void nrio_finish_nand_write(const u32 *spare);
extern void nrio_erase_nand_block(void);
extern int nrio_correct_ecc(const u8 *expected, const u8 *actual, u8 *data);
extern void nrio_select_physical_page(u32 block, u32 page);
extern u8 nrio_check_spare_markers(void);
extern void nrio_set_spare_markers(void);
extern u32 nrio_decode_nand_status(void);

enum {
    NRIO_PAGE_READ_SPARE = 1,
    NRIO_PAGE_READ_SECTOR_ECC = 2,
    NRIO_PAGE_STAGE_SECTOR = 3,
    NRIO_PAGE_PROGRAM_SPARE = 4,
    NRIO_PAGE_WRITE_SECTOR_ECC = 5,
    NRIO_PAGE_RESTORE_SECTOR = 6,
    NRIO_PAGE_ERASE_BLOCK = 7,
    NRIO_PAGE_WRITE_MARKER_PAGE = 8,
    NRIO_PAGE_PAD_SECTOR = 9,
    NRIO_PAGE_READ_PAGE_MARKER = 10,
    NRIO_PAGE_READ_RAW_SECTOR = 11,
};

/* The translation layer reads this same page buffer as 16-bit metadata. */
typedef u32 NandPageWord __attribute__((may_alias));

NRIO_ROUTINE(195c) void nrio_page_operation(u32 operation)
{
    u8 ecc[4];
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif

    /* Case order reproduces the original dispatcher layout under GCC 4.1.1. */
    switch (operation) {
    case NRIO_PAGE_STAGE_SECTOR:
        nrio_read_nand_sector_with_ecc((u8 *)nrio_page_buffer + nrio_sector_in_page * 512,
                                      nrio_sector_in_page, ecc);
#ifdef NRIO_UNIFIED
        if (nrio_bus_fault) return;
#endif
        nrio_correct_ecc(&nrio_spare[nrio_sector_in_page * 16 + 8], ecc,
                         (u8 *)nrio_page_buffer + nrio_sector_in_page * 512);
        break;
    case NRIO_PAGE_PROGRAM_SPARE:
        for (u32 offset = 0; offset != 64; offset += 16) {
            nrio_spare[offset | 4] = nrio_spare[offset | 12] = nrio_spare[offset | 8];
            nrio_spare[offset | 5] = nrio_spare[offset | 13] = nrio_spare[offset | 9];
            nrio_spare[offset | 6] = nrio_spare[offset | 14] = nrio_spare[offset | 10];
        }
        nrio_finish_nand_write((const u32 *)nrio_spare);
        break;
    case NRIO_PAGE_WRITE_SECTOR_ECC:
        nrio_write_nand_sector_with_ecc(nrio_write_buffer, nrio_sector_in_page,
                                        &nrio_spare[nrio_sector_in_page * 16 + 8]);
        break;
    case NRIO_PAGE_RESTORE_SECTOR:
        nrio_write_nand_sector_with_ecc((u8 *)nrio_page_buffer + nrio_sector_in_page * 512,
                                        nrio_sector_in_page,
                                        &nrio_spare[nrio_sector_in_page * 16 + 8]);
        break;
    case NRIO_PAGE_ERASE_BLOCK:
        nrio_erase_nand_block();
        break;
    case NRIO_PAGE_WRITE_MARKER_PAGE:
        for (NandPageWord *word = (NandPageWord *)nrio_page_buffer;
             word < (NandPageWord *)((u8 *)nrio_page_buffer + 512); ++word) {
            *word = 0xffffffff;
        }
        for (u32 offset = 0; offset != 64; offset += 16) {
            nrio_spare[offset | 8] = 0xff;
            nrio_spare[offset | 12] = 0xff;
            nrio_spare[offset | 4] = 0xff;
            nrio_spare[offset | 9] = 0xff;
            nrio_spare[offset | 13] = 0xff;
            nrio_spare[offset | 5] = 0xff;
            nrio_spare[offset | 10] = 0xff;
            nrio_spare[offset | 14] = 0xff;
            nrio_spare[offset | 6] = 0xff;
        }
        nrio_write_nand_sector_raw((u8 *)nrio_page_buffer, 0);
        nrio_write_nand_sector_raw((u8 *)nrio_page_buffer, 1);
        nrio_write_nand_sector_raw((u8 *)nrio_page_buffer, 2);
        nrio_write_nand_sector_raw((u8 *)nrio_page_buffer, 3);
        nrio_finish_nand_write((const u32 *)nrio_spare);
        break;
    case NRIO_PAGE_READ_SPARE:
    case NRIO_PAGE_READ_PAGE_MARKER:
        nrio_read_nand_spare((u32 *)nrio_spare);
        break;
    case NRIO_PAGE_READ_RAW_SECTOR:
        nrio_read_nand_sector((u32 *)nrio_read_buffer, nrio_sector_in_page);
        break;
    case NRIO_PAGE_READ_SECTOR_ECC:
        nrio_read_nand_sector_with_ecc(nrio_read_buffer, nrio_sector_in_page, ecc);
#ifdef NRIO_UNIFIED
        if (nrio_bus_fault) return;
#endif
        nrio_correct_ecc(&nrio_spare[nrio_sector_in_page * 16 + 8], ecc, nrio_read_buffer);
        break;
    case NRIO_PAGE_PAD_SECTOR: {
        u32 sector = nrio_sector_in_page;
        NandPageWord *word = (NandPageWord *)((u8 *)nrio_page_buffer + sector * 512);
        NandPageWord *end = (NandPageWord *)((u8 *)nrio_page_buffer + 512 + sector * 512);
        for (; word < end; ++word) {
            *word = 0xffffffff;
        }
        nrio_write_nand_sector_raw((u8 *)nrio_page_buffer + nrio_sector_in_page * 512,
                                   nrio_sector_in_page);
        u8 *spare = nrio_spare + nrio_sector_in_page * 16;
        spare[8] = 0xff;
        spare[12] = 0xff;
        spare[4] = 0xff;
        spare[9] = 0xff;
        spare[13] = 0xff;
        spare[5] = 0xff;
        spare[10] = 0xff;
        spare[14] = 0xff;
        spare[6] = 0xff;
        break;
    }
    }
}

NRIO_ROUTINE(1ec8) u32 nrio_copy_nand_pages(u8 source, u8 destination, u8 page, u8 end)
{
    if (source != 0xff && page < end) {
        do {
            nrio_select_physical_page(source, page);
            nrio_page_operation(NRIO_PAGE_READ_SPARE);
            if (nrio_check_spare_markers() != 0xff) {
                nrio_sector_in_page = 0;
                do {
                    nrio_page_operation(NRIO_PAGE_STAGE_SECTOR);
                } while (++nrio_sector_in_page < 4);
                nrio_select_physical_page(destination, page);
                nrio_sector_in_page = 0;
                do {
                    nrio_page_operation(NRIO_PAGE_RESTORE_SECTOR);
                } while (++nrio_sector_in_page < 4);

                nrio_spare[11] = nrio_pending_logical_block;
                nrio_set_spare_markers();
                nrio_page_operation(NRIO_PAGE_PROGRAM_SPARE);
                u32 status = nrio_decode_nand_status();
                if (status != 0) {
                    return status;
                }
            }
        } while (++page != end);
    }
    return 0;
}
