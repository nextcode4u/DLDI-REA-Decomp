#include "raw_nand.h"
#ifdef NRIO_RAW_HAS_BANNER
#include "banner_plan.h"
#endif

static u32 sector_count;
static u32 pages_per_block;
static u32 nand_id;

#if defined(RAW_NAND_TOOLKIT)
static enum nrio_raw_mode active_mode;
static bool mode_locked;
#elif defined(RAW_NAND_BANNER)
#define active_mode NRIO_RAW_BANNER_WRITE
#elif defined(RAW_NAND_PROTECT_BOOT)
#define active_mode NRIO_RAW_MAIN_PROTECTED
#elif defined(RAW_NAND_SPARE) && defined(RAW_NAND_WRITE)
#define active_mode NRIO_RAW_SPARE_WRITE
#elif defined(RAW_NAND_SPARE)
#define active_mode NRIO_RAW_SPARE_READ
#elif defined(RAW_NAND_WRITE)
#define active_mode NRIO_RAW_MAIN_WRITE
#else
#define active_mode NRIO_RAW_MAIN_READ
#endif

static bool spare_view(void)
{
    return active_mode == NRIO_RAW_SPARE_READ || active_mode == NRIO_RAW_SPARE_WRITE;
}

static u32 visible_sectors(void)
{
    return spare_view() ? sector_count / (SECTORS_PER_PAGE * SPARES_PER_SECTOR) : sector_count;
}

#ifdef RAW_NAND_WRITE
static u8 *workspace;
static struct nrio_raw_write_error write_error;

static bool writable_mode(void)
{
#ifdef NRIO_RAW_HAS_BANNER
    if (active_mode == NRIO_RAW_BANNER_WRITE) return banner_is_armed();
#endif
    return active_mode >= NRIO_RAW_MAIN_PROTECTED;
}
#endif

bool nrio_raw_startup(void)
{
    sector_count = 0;
#ifdef RAW_NAND_TOOLKIT
    active_mode = NRIO_RAW_MAIN_READ;
    mode_locked = false;
#endif
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
    u32 limit = visible_sectors();
    return sector < limit && count <= limit - sector;
}

bool nrio_raw_read(u32 sector, u32 count, void *buffer)
{
    if (!sector_count || !buffer) {
        return false;
    }
#ifdef RAW_NAND_TOOLKIT
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
#endif
    if (sector == NRIO_RAW_INFO_LBA && count == 0) {
        struct nrio_raw_info *info = buffer;
        info->version = NRIO_RAW_ABI_VERSION;
        info->sectors = visible_sectors();
#ifdef RAW_NAND_WRITE
        info->workspace_bytes = writable_mode() ? pages_per_block * PAGE_RECORD_SIZE : 0;
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
        if (!writable_mode()) return false;
        workspace = buffer;
        return true;
    }
#endif
    if (!valid_range(sector, count)) {
        return false;
    }
#ifdef RAW_NAND_TOOLKIT
    mode_locked = true;
#endif
    u8 *output = buffer;
#if defined(RAW_NAND_SPARE) || defined(RAW_NAND_TOOLKIT)
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
#endif
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
#ifdef RAW_NAND_WRITE
    if (!workspace || !writable_mode() || write_error.stage != NRIO_RAW_WRITE_NONE ||
        !buffer || !valid_range(sector, count)) {
        return false;
    }
    if (active_mode == NRIO_RAW_TABLE_REPAIR) return false;
    // Erasing to update any page of block zero would also erase the boot pages.
    if (active_mode == NRIO_RAW_MAIN_PROTECTED && count && sector < pages_per_block * SECTORS_PER_PAGE) {
        return false;
    }
#ifdef NRIO_RAW_HAS_BANNER
    if (active_mode == NRIO_RAW_BANNER_WRITE && (sector != BANNER_LBA || count != SECTORS_PER_PAGE)) {
        return false;
    }
#endif
    return restore_sectors(sector, count, buffer, pages_per_block, workspace, &write_error, active_mode);
#else
    (void)sector;
    (void)count;
    (void)buffer;
    return false;
#endif
}
