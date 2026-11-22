#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>

#include "boot-repair.h"
#include "session-log.h"

enum { SECTOR_BYTES = 512, SPARE_BYTES = 64, ROM_BLOCK_BYTES = 0x4000, FIRST_UDISK_OFFSET = 0x80000 };

static struct nrio_raw_table_plan plan __attribute__((aligned(4)));
static uint8_t sector[SECTOR_BYTES] __attribute__((aligned(4)));

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool has_data(const uint8_t *bytes, size_t size)
{
    for (size_t i = 1; i < size; ++i) {
        if (bytes[i] != bytes[0]) return true;
    }
    return false;
}

static bool select_view(enum nrio_raw_mode mode, struct nrio_raw_info *info)
{
    return nrio_recovery_select_mode(mode) && nrio_recovery_get_info(info);
}

static bool known_shared_spare(const uint8_t *spare)
{
    const uint8_t *arm7 = spare + SPARE_BYTES;
    const uint8_t *boot_data = spare + 2 * SPARE_BYTES;
    // Restrict the marker exception to the layout seen on the working 16 Gbit cart.
    return spare[0] == 0xff && !has_data(spare, SPARE_BYTES) &&
           read_le32(arm7) == 0xe3a00000 && read_le32(arm7 + 4) == 0xe59f1020 &&
           !memcmp(arm7, spare + 4 * SPARE_BYTES, SPARE_BYTES) &&
           !memcmp(arm7, spare + 5 * SPARE_BYTES, SPARE_BYTES) &&
           has_data(boot_data, 56) && !memcmp(boot_data, spare + 3 * SPARE_BYTES + 8, 56);
}

static uint32_t block_address(uint32_t image_offset)
{
    return read_le32(plan.table + (image_offset / ROM_BLOCK_BYTES) * sizeof(uint32_t));
}

static bool has_mapped_code(uint32_t offset)
{
    uint32_t address = block_address(offset) + offset % ROM_BLOCK_BYTES;
    return nrio_recovery_read(address / SECTOR_BYTES, 1, sector) && has_data(sector, 16);
}

static bool live_udisk(uint32_t capacity)
{
    uint32_t first = block_address(0);
    if (first < FIRST_UDISK_OFFSET || first % ROM_BLOCK_BYTES || first >= capacity) {
        session_log("Invalid first uDisk block\nat 0x%lx.", (unsigned long)first);
        return false;
    }
    if (!nrio_recovery_read(first / SECTOR_BYTES, 1, sector)) {
        session_log("Could not read uDisk header.");
        return false;
    }

    // The released uDisk 1.45 and XuluMenu payloads share this compact header.
    if (read_le32(sector) != 0xea00002e || read_le32(sector + 0x0c) != 0x23232323 ||
        sector[0x14] != 3) {
        session_log("Unrecognized uDisk header\nat 0x%lx.", (unsigned long)first);
        return false;
    }
    uint32_t size = read_le32(sector + 0x80);
    uint32_t arm9 = read_le32(sector + 0x20), arm9_size = read_le32(sector + 0x2c);
    uint32_t arm7 = read_le32(sector + 0x30), arm7_size = read_le32(sector + 0x3c);
    if (size > NRIO_RAW_TABLE_BYTES / sizeof(uint32_t) * ROM_BLOCK_BYTES ||
        arm9 < SECTOR_BYTES || arm9 % SECTOR_BYTES || arm9 >= size || !arm9_size || arm9_size > size - arm9 ||
        arm7 < SECTOR_BYTES || arm7 % SECTOR_BYTES || arm7 >= size || !arm7_size || arm7_size > size - arm7) {
        session_log("Invalid ARM9 or ARM7 range.");
        return false;
    }

    uint32_t previous = 0;
    for (uint32_t offset = 0; offset < size; offset += ROM_BLOCK_BYTES) {
        uint32_t address = block_address(offset);
        if (address < FIRST_UDISK_OFFSET || address % ROM_BLOCK_BYTES ||
            address > capacity - ROM_BLOCK_BYTES || address <= previous) {
            session_log("Invalid map entry %lu: 0x%lx.",
                        (unsigned long)(offset / ROM_BLOCK_BYTES), (unsigned long)address);
            return false;
        }
        previous = address;
    }
    if (!has_mapped_code(arm9) || !has_mapped_code(arm7)) {
        session_log("Mapped ARM9 or ARM7 code\nis missing or unreadable.");
        return false;
    }
    return true;
}

static bool approve_rewrite(const char *title, const char *description)
{
    consoleClear();
    printf("%s\n\n%s\n"
           "Rewrites boot erase block.\n"
           "Keep main and spare backups.\n\n"
           "Hold L + R and press A.\n"
           "B: cancel\n", title, description);
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        uint32_t down = keysDown();
        if (down & KEY_B) return false;
        if ((down & KEY_A) && (keysHeld() & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) return true;
    }
}

void boot_repair(bool check_only)
{
    struct nrio_raw_info spare_info, main_info, repair_info;
    const uint32_t primary_lba = NRIO_RAW_PRIMARY_TABLE / SECTOR_BYTES;
    const uint32_t secondary_lba = NRIO_RAW_SECONDARY_TABLE / SECTOR_BYTES;
    const uint32_t table_sectors = NRIO_RAW_TABLE_BYTES / SECTOR_BYTES;

    session_log("Reading main and spare geometry.");
    if (!select_view(NRIO_RAW_SPARE_READ, &spare_info) || !nrio_recovery_read(0, 1, plan.expected_spare)) {
        session_log("Could not read shared spare data.");
        return;
    }
    nrio_recovery_shutdown();
    if (!nrio_recovery_startup() || !select_view(NRIO_RAW_MAIN_READ, &main_info) ||
        main_info.nand_id != spare_info.nand_id ||
        main_info.sectors <= FIRST_UDISK_OFFSET / SECTOR_BYTES || main_info.sectors > UINT32_MAX / SECTOR_BYTES) {
        session_log("Could not read cart geometry.");
        return;
    }
    session_log("NAND ID %08lx, %lu sectors.", (unsigned long)main_info.nand_id,
                (unsigned long)main_info.sectors);
    session_log("Checking early boot records.");
    if (!known_shared_spare(plan.expected_spare) || !nrio_recovery_read(0, 1, sector) || !has_data(sector, sizeof(sector))) {
        session_log("Early boot data is missing\nor uses an unknown layout.");
        return;
    }
    // Stage 2 skips table candidates preceded by the QAFE marker.
    session_log("Checking table at 0x40000\nand its mapped uDisk image.");
    if (!nrio_recovery_read(secondary_lba - 1, 1, sector)) {
        session_log("Could not read table marker.");
        return;
    }
    if (!memcmp(sector + SECTOR_BYTES - 4, "QAFE", 4)) {
        session_log("Stage 2 would skip this table.");
        return;
    }
    if (!nrio_recovery_read(secondary_lba, table_sectors, plan.table)) {
        session_log("Could not read secondary table.");
        return;
    }
    if (!live_udisk(main_info.sectors * SECTOR_BYTES)) return;

    session_log("uDisk starts at 0x%lx.\nComparing primary table.", (unsigned long)block_address(0));
    uint32_t differing_sectors = 0;
    for (uint32_t i = 0; i < table_sectors; ++i) {
        if (!nrio_recovery_read(primary_lba + i, 1, sector)) {
            session_log("Could not read primary table.");
            return;
        }
        if (memcmp(sector, plan.table + i * SECTOR_BYTES, SECTOR_BYTES)) ++differing_sectors;
    }
    if (!differing_sectors) {
        session_log("Tables already match.");
        return;
    }
    session_log("Preflight passed.\n%lu of %lu table sectors differ.",
                (unsigned long)differing_sectors, (unsigned long)table_sectors);
    if (check_only) {
        session_log("Repair would copy 16 KiB from\n0x40000 to 0x8000. Check only.");
        return;
    }
    if (!approve_rewrite("Repair primary boot table",
                         "Using this cart's uDisk table.\nDirect boot skips safe mode.")) {
        session_log("Repair cancelled.");
        return;
    }

    session_log("Repair confirmed.\nChecking live table and spare\ndata before erase.");
    nrio_recovery_shutdown();
    if (!nrio_recovery_startup() || !select_view(NRIO_RAW_TABLE_REPAIR, &repair_info) ||
        repair_info.nand_id != main_info.nand_id || repair_info.sectors != main_info.sectors ||
        !repair_info.workspace_bytes || repair_info.workspace_bytes > sizeof(raw_workspace) ||
        !nrio_recovery_bind_workspace(raw_workspace, sizeof(raw_workspace)) ||
        !nrio_recovery_repair_table(&plan)) {
        struct nrio_raw_write_error error;
        nrio_recovery_get_write_error(&error);
        if (error.stage) {
            session_log("Repair failed.");
            session_log_write_error(&error);
        } else {
            session_log("Could not start the repair.");
        }
        return;
    }
    session_log("Verified the rewritten block.\nRestart to test uDisk boot.");
}

void install_safe_mode(void)
{
    struct nrio_raw_info info;
    if (!select_view(NRIO_RAW_SAFE_MODE_INSTALL, &info) ||
        !nrio_recovery_bind_workspace(raw_workspace, sizeof(raw_workspace))) {
        session_log("Could not initialize the installer.");
        return;
    }
    const char *description = "Using the prepared backup.";
    char plan_text[80];
    if (!nrio_recovery_safe_mode_ready()) {
        uint32_t sectors[2];
        session_log("Reading block 0 to prepare\nthe stock safe-mode patch.");
        if (!nrio_recovery_prepare_safe_mode(sectors)) {
            struct nrio_raw_write_error error;
            nrio_recovery_get_write_error(&error);
            session_log("Could not prepare stock safe mode.");
            session_log_write_error(&error);
            return;
        }
        snprintf(plan_text, sizeof(plan_text), "Prepared on this DS.\nSectors 0x%lX and 0x%lX.",
                 (unsigned long)sectors[0], (unsigned long)sectors[1]);
        session_log("%s", plan_text);
        description = plan_text;
    }
    if (!approve_rewrite("Install stock safe mode", description)) {
        session_log("Safe-mode install cancelled.");
        return;
    }
    session_log("NAND ID %08lx. Checking the\nentire block-0 backup.",
                (unsigned long)info.nand_id);
    session_log("If it matches, block 0 will\nbe erased, rewritten, verified.");
    if (!nrio_recovery_install_safe_mode()) {
        struct nrio_raw_write_error error;
        nrio_recovery_get_write_error(&error);
        session_log("Safe-mode install failed.");
        session_log_write_error(&error);
        return;
    }
    session_log("Safe-mode install verified.\nRestart on the DSi to test boot.");
}


enum safe_mode_status read_safe_mode_status(struct safe_mode_info *info)
{
    enum { STOCK_ADDRESS = 0x03800000, STAGING_ADDRESS = 0x02380000,
           TABLE_SECTOR = NRIO_RAW_PRIMARY_TABLE / SECTOR_BYTES };
    struct nrio_raw_info nand;
    *info = (struct safe_mode_info){0};
    if (!select_view(NRIO_RAW_MAIN_READ, &nand) ||
        !nrio_recovery_read(TABLE_SECTOR - 1, 1, sector)) return SAFE_MODE_UNREADABLE;
    if (!memcmp(sector + SECTOR_BYTES - 4, "QAFE", 4)) return SAFE_MODE_UNKNOWN;
    if (!nrio_recovery_read(TABLE_SECTOR, 1, sector)) return SAFE_MODE_UNREADABLE;

    uint32_t first = read_le32(sector);
    if (first < ROM_BLOCK_BYTES || first % ROM_BLOCK_BYTES || first / SECTOR_BYTES >= nand.sectors)
        return SAFE_MODE_UNKNOWN;
    info->rom_address = first;
    if (!nrio_recovery_read(first / SECTOR_BYTES, 1, sector)) return SAFE_MODE_UNREADABLE;
    if (read_le32(sector) != 0xea00002e || read_le32(sector + 0x0c) != 0x23232323 ||
        sector[0x12] != 0 || read_le32(sector + 0x84) != SECTOR_BYTES ||
        nrio_safe_mode_crc16(sector, 0x15e) != (sector[0x15e] | sector[0x15f] << 8))
        return SAFE_MODE_UNKNOWN;

    uint32_t source = read_le32(sector + 0x30), entry = read_le32(sector + 0x34);
    uint32_t destination = read_le32(sector + 0x38), size = read_le32(sector + 0x3c);
    uint32_t used_size = read_le32(sector + 0x80);
    uint32_t map_offset = source / ROM_BLOCK_BYTES * sizeof(uint32_t);
    if (source < SECTOR_BYTES || source % SECTOR_BYTES || source >= used_size ||
        size > used_size - source || map_offset >= 0x10000 || entry != destination)
        return SAFE_MODE_UNKNOWN;
    bool stock = destination == STOCK_ADDRESS && size == 0x38;
    if (!stock && (destination != STAGING_ADDRESS || size != SECTOR_BYTES))
        return SAFE_MODE_UNKNOWN;

    if (!nrio_recovery_read(TABLE_SECTOR + map_offset / SECTOR_BYTES, 1, sector))
        return SAFE_MODE_UNREADABLE;
    uint32_t mapped = read_le32(sector + map_offset % SECTOR_BYTES);
    if (mapped < ROM_BLOCK_BYTES || mapped % ROM_BLOCK_BYTES)
        return SAFE_MODE_UNKNOWN;
    uint32_t arm7_sector = mapped / SECTOR_BYTES + source % ROM_BLOCK_BYTES / SECTOR_BYTES;
    if (arm7_sector >= nand.sectors) return SAFE_MODE_UNKNOWN;
    info->arm7_address = arm7_sector * SECTOR_BYTES;
    if (!nrio_recovery_read(arm7_sector, 1, sector)) return SAFE_MODE_UNREADABLE;

    if (stock) {
        info->handoff = nrio_safe_mode_handoff(sector);
        return info->handoff ? SAFE_MODE_STOCK : SAFE_MODE_UNKNOWN;
    }
    // Compare the entire stub prefix, including addresses, before checking its payload.
    if (nrio_safe_mode_crc16(sector, 0x6c) == 0x1a2d) {
        info->handoff = nrio_safe_mode_handoff(sector + 0x6c);
        return info->handoff ? SAFE_MODE_TOOLKIT : SAFE_MODE_UNKNOWN;
    }
    if (nrio_safe_mode_crc16(sector, 0x3c) == 0x2382 &&
        read_le32(sector + 0x74) == 0x38 && read_le32(sector + 0x78) == STOCK_ADDRESS) {
        info->handoff = nrio_safe_mode_handoff(sector + 0x3c);
        return info->handoff ? SAFE_MODE_APACHE : SAFE_MODE_UNKNOWN;
    }
    return SAFE_MODE_UNKNOWN;
}

void check_safe_mode(void)
{
    static const char *const descriptions[] = {
        [SAFE_MODE_UNKNOWN] = "Unknown safe-mode layout.",
        [SAFE_MODE_UNREADABLE] = "Could not read the safe-mode map or ROM.",
        [SAFE_MODE_STOCK] = "Supported stock safe-mode ROM.",
        [SAFE_MODE_TOOLKIT] = "Toolkit DSi ARM7 patch found.",
        [SAFE_MODE_APACHE] = "Apache's DSi ARM7 patch found.",
    };
    struct safe_mode_info info;
    session_log("Reading the primary boot table.");
    enum safe_mode_status status = read_safe_mode_status(&info);
    session_log("%s", descriptions[status]);
    if (info.rom_address)
        session_log("ROM: NAND 0x%08lx.", (unsigned long)info.rom_address);
    if (info.arm7_address)
        session_log("ARM7: NAND 0x%08lx.", (unsigned long)info.arm7_address);
    if (info.handoff)
        session_log("ARM7 handoff: 0x%08lx.", (unsigned long)info.handoff);
    session_log("Read only. No NAND writes.");
}
