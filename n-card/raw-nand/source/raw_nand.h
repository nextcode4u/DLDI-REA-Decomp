#ifndef NRIO_RAW_NAND_H
#define NRIO_RAW_NAND_H

#include <nds.h>

#include "../usb/raw-dldi.h"

#if defined(RAW_NAND_BANNER) || defined(RAW_NAND_TOOLKIT)
#define NRIO_RAW_HAS_BANNER
#endif

enum {
    SECTOR_SIZE = 512,
    SECTORS_PER_PAGE = 4,
    PAGE_SIZE = SECTOR_SIZE * SECTORS_PER_PAGE,
    SPARE_SIZE = 64,
    SPARES_PER_SECTOR = SECTOR_SIZE / SPARE_SIZE,
    PAGE_RECORD_SIZE = PAGE_SIZE + SPARE_SIZE,
};

bool nand_read_id(u32 *id);
bool nand_load_page(u32 page, bool spare);
bool nand_read_sector(u32 column, u8 *buffer);
bool nand_read_spare(u8 *buffer);

#ifdef RAW_NAND_WRITE
bool nand_erase_block(u32 page);
bool nand_program_page(u32 page, const u8 *record);
bool restore_sectors(u32 sector, u32 count, const u8 *buffer, u32 pages_per_block, u8 *workspace,
                     struct nrio_raw_write_error *error, enum nrio_raw_mode mode);
#ifdef RAW_NAND_TOOLKIT
bool restore_boot_table(const struct nrio_raw_table_plan *plan, u32 pages_per_block, u8 *workspace,
                        struct nrio_raw_write_error *error);
#endif
#endif

#endif
