#include "raw_nand.h"
#include "banner_plan.h"

static u32 sector_count;
static u32 pages_per_block;
static u32 nand_id;

static enum nrio_raw_mode active_mode;
static bool mode_locked;

static bool spare_view(void)
{
    return active_mode == NRIO_RAW_SPARE_READ || active_mode == NRIO_RAW_SPARE_WRITE;
}

static u32 visible_sectors(void)
{
    return spare_view() ? sector_count / (SECTORS_PER_PAGE * SPARES_PER_SECTOR) : sector_count;
}

static u8 *workspace;
static struct nrio_raw_write_error write_error;

static bool writable_mode(void)
{
    if (active_mode == NRIO_RAW_BANNER_WRITE) return banner_is_armed();
    return active_mode >= NRIO_RAW_MAIN_PROTECTED;
}

bool nrio_raw_startup(void)
{
    sector_count = 0;
    active_mode = NRIO_RAW_MAIN_READ;
    mode_locked = false;
    workspace = 0;
    write_error.stage = NRIO_RAW_WRITE_NONE;
    write_error.page = 0;
    write_error.detail = 0;

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
    return sector_count != 0;
}

bool nrio_raw_inserted(void)
{
    return sector_count != 0;
}

bool nrio_raw_clear(void)
{
    return write_error.stage == NRIO_RAW_WRITE_NONE;
}

bool nrio_raw_shutdown(void)
{
    sector_count = 0;
    workspace = 0;
    return true;
}

static bool valid_range(u32 sector, u32 count)
{
    u32 limit = visible_sectors();
    return sector < limit && count <= limit - sector;
}

bool nrio_raw_read(u32 sector, u32 count, void *buffer)
{
    if (!sector_count || !buffer) {
        return false;
    }
    if (sector == NRIO_RAW_MODE_LBA && count == 0 && (u32)buffer % 4 == 0) {
        u32 mode = *(const u32 *)buffer;
        if (mode_locked || mode > NRIO_RAW_TABLE_REPAIR ||
            (mode == NRIO_RAW_BANNER_WRITE && !banner_is_armed())) return false;
        active_mode = mode;
        mode_locked = true;
        return true;
    }
    if (sector == NRIO_RAW_REPAIR_TABLE_LBA && count == 0 && (u32)buffer % 4 == 0) {
        if (active_mode != NRIO_RAW_TABLE_REPAIR || !workspace || write_error.stage != NRIO_RAW_WRITE_NONE)
            return false;
        return restore_boot_table(buffer, pages_per_block, workspace, &write_error);
    }
    if (sector == NRIO_RAW_INFO_LBA && count == 0) {
        struct nrio_raw_info *info = buffer;
        info->version = NRIO_RAW_ABI_VERSION;
        info->sectors = visible_sectors();
        info->workspace_bytes = writable_mode() ? pages_per_block * PAGE_RECORD_SIZE : 0;
        info->nand_id = nand_id;
        return true;
    }
    if (sector == NRIO_RAW_WRITE_ERROR_LBA && count == 0) {
        struct nrio_raw_write_error *result = buffer;
        *result = write_error;
        return true;
    }
    if (sector == NRIO_RAW_WORKSPACE_LBA && count >= pages_per_block * PAGE_RECORD_SIZE &&
        (u32)buffer % 4 == 0) {
        if (!writable_mode()) return false;
        workspace = buffer;
        return true;
    }
    if (!valid_range(sector, count)) {
        return false;
    }
    mode_locked = true;
    u8 *output = buffer;
    if (spare_view()) {
        u32 page = sector * SPARES_PER_SECTOR;
        for (u32 i = 0; i < count * SPARES_PER_SECTOR; ++i) {
            if (!nand_load_page(page + i, true) || !nand_read_spare(output)) {
                return false;
            }
            output += SPARE_SIZE;
        }
        return true;
    }
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
    return true;
}

bool nrio_raw_write(u32 sector, u32 count, const void *buffer)
{
    if (!workspace || !writable_mode() || write_error.stage != NRIO_RAW_WRITE_NONE ||
        !buffer || !valid_range(sector, count)) {
        return false;
    }
    if (active_mode == NRIO_RAW_TABLE_REPAIR) return false;
    // Erasing to update any page of block zero would also erase the boot pages.
    if (active_mode == NRIO_RAW_MAIN_PROTECTED && count && sector < pages_per_block * SECTORS_PER_PAGE) {
        return false;
    }
    if (active_mode == NRIO_RAW_BANNER_WRITE && (sector != BANNER_LBA || count != SECTORS_PER_PAGE)) {
        return false;
    }
    return restore_sectors(sector, count, buffer, pages_per_block, workspace, &write_error, active_mode);
}
