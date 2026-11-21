#include "raw_nand.h"
#include "banner_plan.h"
#include "recovery.h"
#include "../../source/card/nand_ecc.h"

static u32 load_word(const u8 *in)
{
    return in[0] | ((u32)in[1] << 8) | ((u32)in[2] << 16) | ((u32)in[3] << 24);
}

static void regenerate_ecc(u8 *record, u32 column)
{
    const u8 *data = record + column * SECTOR_SIZE;
    u32 all = 0, bit0 = 0, bit1 = 0, bit2 = 0, row = 0;
    bool odd = false;
    for (u32 group = 0; group < 16; ++group) {
        u32 parity = 0;
        for (u32 i = 0; i < 8; ++i) {
            u32 word = load_word(data);
            data += 4;
            parity ^= word;
            if (i & 1) bit0 ^= word;
            if (i & 2) bit1 ^= word;
            if (i & 4) bit2 ^= word;
        }
        all ^= parity;
        if (odd_parity(parity)) {
            row ^= group;
            odd = !odd;
        }
    }

    u8 *spare = record + PAGE_SIZE + column * 16;
    finish_ecc(spare + 8, all, bit0, bit1, bit2, row, odd);
    for (u32 i = 0; i < 3; ++i) {
        spare[4 + i] = spare[8 + i];
        spare[12 + i] = spare[8 + i];
    }
}

// Patched offline in the shipped template. Keep the plan local for PC-relative access.
static volatile struct nrio_banner_plan nrio_banner_plan = {
    .signature = "NRIO-BANNER-v1",
};

bool banner_is_armed(void)
{
    return nrio_banner_plan.armed == 1;
}

static bool read_page(u32 page, u8 *record)
{
    if (!nand_load_page(page, false)) {
        return false;
    }
    for (u32 i = 0; i < SECTORS_PER_PAGE; ++i) {
        if (!nand_read_sector(i, record + i * SECTOR_SIZE)) {
            return false;
        }
    }
    return nand_load_page(page, true) && nand_read_spare(record + PAGE_SIZE);
}

static bool equal(const u8 *a, const volatile u8 *b, u32 size)
{
    for (u32 i = 0; i < size; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

static bool erased(const u8 *record)
{
    for (u32 i = 0; i < PAGE_RECORD_SIZE; ++i) {
        if (record[i] != 0xff) return false;
    }
    return true;
}

static bool fail_write(struct nrio_raw_write_error *error, u32 stage, u32 page, u32 detail)
{
    error->stage = stage;
    error->page = page;
    error->detail = detail;
    return false;
}

static bool rewrite_block(u32 page, u32 pages, u32 offset, u32 count,
                          const u8 *input, u8 *records, struct nrio_raw_write_error *error,
                          enum nrio_raw_mode mode, const u8 *expected_spare)
{
    bool spare = mode == NRIO_RAW_SPARE_WRITE;
    u32 update_size = spare ? SPARE_SIZE : SECTOR_SIZE;
    u32 update_offset = spare ? PAGE_SIZE : 0;
    bool banner = mode == NRIO_RAW_BANNER_WRITE;
    if (banner && (page != 0 || offset != BANNER_LBA || count != SECTORS_PER_PAGE ||
                   !equal(input, nrio_banner_plan.replacement_main, PAGE_SIZE))) return false;
    for (u32 i = 0; i < pages; ++i) {
        u8 *record = records + i * PAGE_RECORD_SIZE;
        if (!read_page(page + i, record))
            return fail_write(error, NRIO_RAW_WRITE_READ_BLOCK, page + i, 0);
        if (expected_spare) {
            if (i < SPARES_PER_SECTOR && !equal(record + PAGE_SIZE, expected_spare + i * SPARE_SIZE, SPARE_SIZE))
                return fail_write(error, NRIO_RAW_WRITE_MARKER, page + i, record[PAGE_SIZE]);
            continue;
        }
        if (banner) {
            if (i < BANNER_EXPECTED_PAGES &&
                !equal(record + PAGE_SIZE, nrio_banner_plan.expected_spare[i], SPARE_SIZE))
                return fail_write(error, NRIO_RAW_WRITE_MARKER, page + i, record[PAGE_SIZE]);
            continue;
        }
        // Refuse unfamiliar markers on the first two physical pages.
        u8 marker = record[PAGE_SIZE];
        if (i < 2 && marker != 0xff && marker != 0x55)
            return fail_write(error, NRIO_RAW_WRITE_MARKER, page + i, marker);
    }

    bool changed = false;
    for (u32 i = 0; i < count; ++i) {
        u32 index = offset + i;
        u32 column = spare ? 0 : index % SECTORS_PER_PAGE;
        u32 page_index = spare ? index : index / SECTORS_PER_PAGE;
        u8 *record = records + page_index * PAGE_RECORD_SIZE;
        u8 *data = record + update_offset + column * update_size;
        const u8 *source = input + i * update_size;
        if (!equal(data, source, update_size)) {
            for (u32 j = 0; j < update_size; ++j) data[j] = source[j];
            if (!spare && mode != NRIO_RAW_BANNER_WRITE) regenerate_ecc(record, column);
            changed = true;
        }
    }
    u8 *banner_spare = records + BANNER_PAGE * PAGE_RECORD_SIZE + PAGE_SIZE;
    if (banner && !equal(banner_spare, nrio_banner_plan.replacement_spare, SPARE_SIZE)) {
        for (u32 i = 0; i < SPARE_SIZE; ++i) banner_spare[i] = nrio_banner_plan.replacement_spare[i];
        changed = true;
    }
    if (!changed) return true;

    if (!nand_erase_block(page))
        return fail_write(error, NRIO_RAW_WRITE_ERASE, page, 0);
    for (u32 i = 0; i < pages; ++i) {
        const u8 *record = records + i * PAGE_RECORD_SIZE;
        if (!erased(record) && !nand_program_page(page + i, record))
            return fail_write(error, NRIO_RAW_WRITE_PROGRAM, page + i, 0);
    }

    u8 verified[PAGE_RECORD_SIZE];
    for (u32 i = 0; i < pages; ++i) {
        if (!read_page(page + i, verified))
            return fail_write(error, NRIO_RAW_WRITE_VERIFY_READ, page + i, 0);
        if (!equal(verified, records + i * PAGE_RECORD_SIZE, PAGE_RECORD_SIZE))
            return fail_write(error, NRIO_RAW_WRITE_VERIFY_DATA, page + i, 0);
    }
    return true;
}

bool restore_sectors(u32 sector, u32 count, const u8 *buffer, u32 pages_per_block, u8 *workspace,
                     struct nrio_raw_write_error *error, enum nrio_raw_mode mode)
{
    bool spare = mode == NRIO_RAW_SPARE_WRITE;
    u32 updates_per_sector = spare ? SPARES_PER_SECTOR : 1;
    u32 block_sectors = spare ? pages_per_block / SPARES_PER_SECTOR : pages_per_block * SECTORS_PER_PAGE;
    while (count) {
        u32 offset = sector & (block_sectors - 1);
        u32 batch = block_sectors - offset;
        if (batch > count) batch = count;
        u32 first_sector = sector - offset;
        u32 page = spare ? first_sector * SPARES_PER_SECTOR : first_sector / SECTORS_PER_PAGE;
        if (!rewrite_block(page, pages_per_block, offset * updates_per_sector, batch * updates_per_sector,
                           buffer, workspace, error, mode, 0)) return false;
        sector += batch;
        count -= batch;
        buffer += batch * SECTOR_SIZE;
    }
    return true;
}

bool restore_boot_table(const struct nrio_raw_table_plan *plan, u32 pages_per_block, u8 *workspace,
                        struct nrio_raw_write_error *error)
{
    if (plan->expected_spare[0] != 0xff)
        return fail_write(error, NRIO_RAW_WRITE_MARKER, 0, plan->expected_spare[0]);
    if (plan->expected_spare[SPARE_SIZE] != 0)
        return fail_write(error, NRIO_RAW_WRITE_MARKER, 1, plan->expected_spare[SPARE_SIZE]);

    u8 sector[SECTOR_SIZE];
    for (u32 offset = 0; offset < NRIO_RAW_TABLE_BYTES; offset += PAGE_SIZE) {
        u32 page = (NRIO_RAW_SECONDARY_TABLE + offset) / PAGE_SIZE;
        if (!nand_load_page(page, false))
            return fail_write(error, NRIO_RAW_WRITE_TABLE_SOURCE, page, 0);
        for (u32 column = 0; column < SECTORS_PER_PAGE; ++column) {
            if (!nand_read_sector(column, sector) ||
                !equal(sector, plan->table + offset + column * SECTOR_SIZE, SECTOR_SIZE))
                return fail_write(error, NRIO_RAW_WRITE_TABLE_SOURCE, page, column);
        }
    }

    // Both supported erase geometries place the entire primary table in block zero.
    return rewrite_block(0, pages_per_block, NRIO_RAW_PRIMARY_TABLE / SECTOR_SIZE,
                         NRIO_RAW_TABLE_BYTES / SECTOR_SIZE, plan->table, workspace, error,
                         NRIO_RAW_TABLE_REPAIR, plan->expected_spare);
}
