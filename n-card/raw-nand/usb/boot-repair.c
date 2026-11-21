#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>
#include <nds/arm9/dldi.h>

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

static bool select_view(const DISC_INTERFACE *io, uint32_t mode, struct nrio_raw_info *info)
{
    return io->isInserted() && io->readSectors(NRIO_RAW_MODE_LBA, 0, &mode) &&
           io->readSectors(NRIO_RAW_INFO_LBA, 0, info) && info->version == NRIO_RAW_ABI_VERSION;
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

static bool has_mapped_code(const DISC_INTERFACE *io, uint32_t offset)
{
    uint32_t address = block_address(offset) + offset % ROM_BLOCK_BYTES;
    return io->readSectors(address / SECTOR_BYTES, 1, sector) && has_data(sector, 16);
}

static bool live_udisk(const DISC_INTERFACE *io, uint32_t capacity)
{
    uint32_t first = block_address(0);
    if (first < FIRST_UDISK_OFFSET || first % ROM_BLOCK_BYTES || first >= capacity) {
        session_log("Invalid first uDisk block\nat 0x%lx.", (unsigned long)first);
        return false;
    }
    if (!io->readSectors(first / SECTOR_BYTES, 1, sector)) {
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
    if (!has_mapped_code(io, arm9) || !has_mapped_code(io, arm7)) {
        session_log("Mapped ARM9 or ARM7 code\nis missing or unreadable.");
        return false;
    }
    return true;
}

static bool approve_repair(void)
{
    consoleClear();
    printf("Repair primary boot table\n\n"
           "Using this cart's uDisk table.\n"
           "Direct boot skips safe mode.\n"
           "Rewrites boot erase block.\n"
           "Keep main and spare backups.\n\n"
           "Hold L + R and press A.\n"
           "B: cancel\n");
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        uint32_t down = keysDown();
        if (down & KEY_B) return false;
        if ((down & KEY_A) && (keysHeld() & (KEY_L | KEY_R)) == (KEY_L | KEY_R)) return true;
    }
}

void boot_repair(void)
{
    const DISC_INTERFACE *io = dldiGetInternal();
    struct nrio_raw_info spare_info, main_info, repair_info;
    const uint32_t primary_lba = NRIO_RAW_PRIMARY_TABLE / SECTOR_BYTES;
    const uint32_t secondary_lba = NRIO_RAW_SECONDARY_TABLE / SECTOR_BYTES;
    const uint32_t table_sectors = NRIO_RAW_TABLE_BYTES / SECTOR_BYTES;

    session_log("Reading main and spare geometry.");
    if (memcmp(&io->ioType, "NRWR", 4) || !select_view(io, NRIO_RAW_SPARE_READ, &spare_info) ||
        !io->readSectors(0, 1, plan.expected_spare) || !io->shutdown() || !io->startup() ||
        !select_view(io, NRIO_RAW_MAIN_READ, &main_info) || main_info.nand_id != spare_info.nand_id ||
        main_info.sectors <= FIRST_UDISK_OFFSET / SECTOR_BYTES || main_info.sectors > UINT32_MAX / SECTOR_BYTES) {
        session_log("Could not read cart geometry.");
        return;
    }
    session_log("NAND ID %08lx, %lu sectors.", (unsigned long)main_info.nand_id,
                (unsigned long)main_info.sectors);
    session_log("Checking early boot records.");
    if (!known_shared_spare(plan.expected_spare) || !io->readSectors(0, 1, sector) || !has_data(sector, sizeof(sector))) {
        session_log("Early boot data is missing\nor uses an unknown layout.");
        return;
    }
    // Stage 2 skips table candidates preceded by the QAFE marker.
    session_log("Checking table at 0x40000\nand its mapped uDisk image.");
    if (!io->readSectors(secondary_lba - 1, 1, sector)) {
        session_log("Could not read table marker.");
        return;
    }
    if (!memcmp(sector + SECTOR_BYTES - 4, "QAFE", 4)) {
        session_log("Stage 2 would skip this table.");
        return;
    }
    if (!io->readSectors(secondary_lba, table_sectors, plan.table)) {
        session_log("Could not read secondary table.");
        return;
    }
    if (!live_udisk(io, main_info.sectors * SECTOR_BYTES)) return;

    session_log("uDisk starts at 0x%lx.\nComparing primary table.", (unsigned long)block_address(0));
    bool identical = true;
    for (uint32_t i = 0; i < table_sectors; ++i) {
        if (!io->readSectors(primary_lba + i, 1, sector)) {
            session_log("Could not read primary table.");
            return;
        }
        if (memcmp(sector, plan.table + i * SECTOR_BYTES, SECTOR_BYTES)) identical = false;
    }
    if (identical) {
        session_log("Tables already match.");
        return;
    }
    session_log("Preflight passed. Tables differ.");
    if (!approve_repair()) {
        session_log("Repair cancelled.");
        return;
    }

    session_log("Repair confirmed.\nChecking live table and spare\ndata before erase.");
    if (!io->shutdown() || !io->startup() || !select_view(io, NRIO_RAW_TABLE_REPAIR, &repair_info) ||
        repair_info.nand_id != main_info.nand_id || repair_info.sectors != main_info.sectors ||
        !repair_info.workspace_bytes || repair_info.workspace_bytes > sizeof(raw_workspace) ||
        !io->readSectors(NRIO_RAW_WORKSPACE_LBA, sizeof(raw_workspace), raw_workspace) ||
        !io->readSectors(NRIO_RAW_REPAIR_TABLE_LBA, 0, &plan)) {
        struct nrio_raw_write_error error;
        if (io->readSectors(NRIO_RAW_WRITE_ERROR_LBA, 0, &error) && error.stage) {
            session_log("Repair failed.");
            session_log_write_error(&error);
        } else {
            session_log("Could not start the repair.");
        }
        return;
    }
    session_log("Verified the rewritten block.\nRestart to test uDisk boot.");
}
