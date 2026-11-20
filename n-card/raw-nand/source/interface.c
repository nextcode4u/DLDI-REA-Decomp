#include "raw_nand.h"

static u32 sector_count;
static u32 pages_per_block;
static u32 nand_id;

#ifdef RAW_NAND_WRITE
static u8 *workspace;
static struct nrio_raw_write_error write_error;
#endif

bool nrio_raw_startup(void)
{
    sector_count = 0;
#ifdef RAW_NAND_WRITE
    workspace = 0;
    write_error.stage = NRIO_RAW_WRITE_NONE;
    write_error.page = 0;
    write_error.detail = 0;
#endif

    if (!nand_read_id(&nand_id)) {
        return false;
    }
    u8 maker = nand_id;
    if (maker != 0xec && maker != 0xad && maker != 0x2c && maker != 0x98) {
        return false;
    }

    // The original NRIO geometry subtracts 64 blocks for the hidden region.
    // Recover the full physical capacity by using the complete block count.
    u8 device = nand_id >> 8;
    u32 sectors_per_128_mib = 128 * 1024 * 1024 / SECTOR_SIZE;
    if (device == 0x74 || device == 0x79 || device == 0xc1 || device == 0xf1) {
        sector_count = sectors_per_128_mib;
    } else if (device == 0x71 || device == 0xca || device == 0xda) {
        sector_count = sectors_per_128_mib * 2;
    } else if (device == 0xcc || device == 0xdc) {
        sector_count = sectors_per_128_mib * 4;
    } else if (device == 0xc3 || device == 0xd3) {
        sector_count = sectors_per_128_mib * 8;
    } else if (device == 0xc5 || device == 0xd5) {
        sector_count = sectors_per_128_mib * 16;
    }
    pages_per_block = nand_id & (0x20u << 24) ? 128 : 64;
#ifdef RAW_NAND_SPARE
    sector_count /= SECTORS_PER_PAGE * SPARES_PER_SECTOR;
#endif
    return sector_count != 0;
}

bool nrio_raw_inserted(void)
{
    return sector_count != 0;
}

bool nrio_raw_clear(void)
{
#ifdef RAW_NAND_WRITE
    return write_error.stage == NRIO_RAW_WRITE_NONE;
#else
    return true;
#endif
}

bool nrio_raw_shutdown(void)
{
    sector_count = 0;
#ifdef RAW_NAND_WRITE
    workspace = 0;
#endif
    return true;
}

static bool valid_range(u32 sector, u32 count)
{
    return sector < sector_count && count <= sector_count - sector;
}

bool nrio_raw_read(u32 sector, u32 count, void *buffer)
{
    if (!sector_count || !buffer) {
        return false;
    }
    if (sector == NRIO_RAW_INFO_LBA && count == 0) {
        struct nrio_raw_info *info = buffer;
        info->version = NRIO_RAW_ABI_VERSION;
        info->sectors = sector_count;
#ifdef RAW_NAND_WRITE
        info->workspace_bytes = pages_per_block * PAGE_RECORD_SIZE;
#else
        info->workspace_bytes = 0;
#endif
        info->nand_id = nand_id;
        return true;
    }
#ifdef RAW_NAND_WRITE
    if (sector == NRIO_RAW_WRITE_ERROR_LBA && count == 0) {
        struct nrio_raw_write_error *result = buffer;
        *result = write_error;
        return true;
    }
    if (sector == NRIO_RAW_WORKSPACE_LBA && count >= pages_per_block * PAGE_RECORD_SIZE &&
        (u32)buffer % 4 == 0) {
        workspace = buffer;
        return true;
    }
#endif
    if (!valid_range(sector, count)) {
        return false;
    }
    u8 *output = buffer;
#ifdef RAW_NAND_SPARE
    u32 page = sector * SPARES_PER_SECTOR;
    for (u32 i = 0; i < count * SPARES_PER_SECTOR; ++i) {
        if (!nand_load_page(page + i, true) || !nand_read_spare(output)) {
            return false;
        }
        output += SPARE_SIZE;
    }
#else
    u32 loaded_page = (u32)-1;
    while (count--) {
        u32 page = sector / SECTORS_PER_PAGE;
        if (page != loaded_page && !nand_load_page(page, false)) {
            return false;
        }
        loaded_page = page;
        if (!nand_read_sector(sector % SECTORS_PER_PAGE, output)) {
            return false;
        }
        output += SECTOR_SIZE;
        ++sector;
    }
#endif
    return true;
}

bool nrio_raw_write(u32 sector, u32 count, const void *buffer)
{
#ifdef RAW_NAND_WRITE
    if (!workspace || write_error.stage != NRIO_RAW_WRITE_NONE || !buffer || !valid_range(sector, count)) {
        return false;
    }
    return restore_sectors(sector, count, buffer, pages_per_block, workspace, &write_error);
#else
    (void)sector;
    (void)count;
    (void)buffer;
    return false;
#endif
}
