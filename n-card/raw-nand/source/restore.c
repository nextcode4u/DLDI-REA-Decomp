#include "raw_nand.h"
#ifdef RAW_NAND_BANNER
#include "banner_plan.h"
#endif
#ifdef RAW_NAND_SPARE
enum { UPDATE_SIZE = SPARE_SIZE, UPDATES_PER_PAGE = 1, UPDATE_OFFSET = PAGE_SIZE };
#else
enum { UPDATE_SIZE = SECTOR_SIZE, UPDATES_PER_PAGE = SECTORS_PER_PAGE, UPDATE_OFFSET = 0 };
#endif
#if !defined(RAW_NAND_SPARE) && !defined(RAW_NAND_BANNER)
#include "../../source/card/nand_ecc.h"

static u32 load_word(const u8 *in)
{
    return in[0] | ((u32)in[1] << 8) | ((u32)in[2] << 16) | ((u32)in[3] << 24);
}

static void regenerate_ecc(u8 *record, u32 column)
{
    const u8 *data = record + UPDATE_OFFSET + column * UPDATE_SIZE;
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

#endif

enum { UPDATES_PER_SECTOR = SECTOR_SIZE / UPDATE_SIZE };

#ifdef RAW_NAND_BANNER
// Patched offline in the shipped template. Keep the plan local for PC-relative access.
static volatile struct nrio_banner_plan nrio_banner_plan = {
    .signature = "NRIO-BANNER-v1",
};

bool banner_is_armed(void)
{
    return nrio_banner_plan.armed == 1;
}
#endif

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

#ifdef RAW_NAND_BANNER
static bool equal(const u8 *a, const volatile u8 *b, u32 size)
#else
static bool equal(const u8 *a, const u8 *b, u32 size)
#endif
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
                          const u8 *input, u8 *records, struct nrio_raw_write_error *error)
{
#ifdef RAW_NAND_BANNER
    if (page != 0 || offset != BANNER_LBA || count != SECTORS_PER_PAGE ||
        !equal(input, nrio_banner_plan.replacement_main, PAGE_SIZE)) return false;
#endif
    for (u32 i = 0; i < pages; ++i) {
        u8 *record = records + i * PAGE_RECORD_SIZE;
        if (!read_page(page + i, record))
            return fail_write(error, NRIO_RAW_WRITE_READ_BLOCK, page + i, 0);
#ifdef RAW_NAND_BANNER
        if (i < BANNER_EXPECTED_PAGES &&
            !equal(record + PAGE_SIZE, nrio_banner_plan.expected_spare[i], SPARE_SIZE))
            return fail_write(error, NRIO_RAW_WRITE_MARKER, page + i, record[PAGE_SIZE]);
#else
        // Refuse unfamiliar markers on the first two physical pages.
        u8 marker = record[PAGE_SIZE];
        if (i < 2 && marker != 0xff && marker != 0x55)
            return fail_write(error, NRIO_RAW_WRITE_MARKER, page + i, marker);
#endif
    }

    bool changed = false;
    for (u32 i = 0; i < count; ++i) {
        u32 index = offset + i;
        u32 column = index % UPDATES_PER_PAGE;
        u8 *record = records + (index / UPDATES_PER_PAGE) * PAGE_RECORD_SIZE;
        u8 *data = record + UPDATE_OFFSET + column * UPDATE_SIZE;
        const u8 *source = input + i * UPDATE_SIZE;
        if (!equal(data, source, UPDATE_SIZE)) {
            for (u32 j = 0; j < UPDATE_SIZE; ++j) data[j] = source[j];
#if !defined(RAW_NAND_SPARE) && !defined(RAW_NAND_BANNER)
            regenerate_ecc(record, column);
#endif
            changed = true;
        }
    }
#ifdef RAW_NAND_BANNER
    u8 *banner_spare = records + BANNER_PAGE * PAGE_RECORD_SIZE + PAGE_SIZE;
    if (!equal(banner_spare, nrio_banner_plan.replacement_spare, SPARE_SIZE)) {
        for (u32 i = 0; i < SPARE_SIZE; ++i) banner_spare[i] = nrio_banner_plan.replacement_spare[i];
        changed = true;
    }
#endif
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
                     struct nrio_raw_write_error *error)
{
    u32 block_sectors = pages_per_block * UPDATES_PER_PAGE / UPDATES_PER_SECTOR;
    while (count) {
        u32 offset = sector & (block_sectors - 1);
        u32 batch = block_sectors - offset;
        if (batch > count) batch = count;
        if (!rewrite_block((sector - offset) * UPDATES_PER_SECTOR / UPDATES_PER_PAGE,
                           pages_per_block, offset * UPDATES_PER_SECTOR,
                           batch * UPDATES_PER_SECTOR, buffer, workspace, error)) return false;
        sector += batch;
        count -= batch;
        buffer += batch * SECTOR_SIZE;
    }
    return true;
}
