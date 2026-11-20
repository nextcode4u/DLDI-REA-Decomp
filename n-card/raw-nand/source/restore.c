#include "raw_nand.h"
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

static bool equal(const u8 *a, const u8 *b, u32 size)
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

static bool rewrite_block(u32 page, u32 pages, u32 offset, u32 count,
                          const u8 *input, u8 *records)
{
    for (u32 i = 0; i < pages; ++i) {
        u8 *record = records + i * PAGE_RECORD_SIZE;
        if (!read_page(page + i, record)) return false;
        // Refuse unfamiliar markers on the first two physical pages.
        u8 marker = record[PAGE_SIZE];
        if (i < 2 && marker != 0xff && marker != 0x55) return false;
    }

    bool changed = false;
    for (u32 i = 0; i < count; ++i) {
        u32 index = offset + i;
        u32 column = index % SECTORS_PER_PAGE;
        u8 *record = records + (index / SECTORS_PER_PAGE) * PAGE_RECORD_SIZE;
        u8 *data = record + column * SECTOR_SIZE;
        const u8 *source = input + i * SECTOR_SIZE;
        if (!equal(data, source, SECTOR_SIZE)) {
            for (u32 j = 0; j < SECTOR_SIZE; ++j) data[j] = source[j];
            regenerate_ecc(record, column);
            changed = true;
        }
    }
    if (!changed) return true;

    if (!nand_erase_block(page)) return false;
    for (u32 i = 0; i < pages; ++i) {
        const u8 *record = records + i * PAGE_RECORD_SIZE;
        if (!erased(record) && !nand_program_page(page + i, record)) return false;
    }

    u8 verified[PAGE_RECORD_SIZE];
    for (u32 i = 0; i < pages; ++i) {
        if (!read_page(page + i, verified) ||
            !equal(verified, records + i * PAGE_RECORD_SIZE, PAGE_RECORD_SIZE)) return false;
    }
    return true;
}

bool restore_sectors(u32 sector, u32 count, const u8 *buffer, u32 pages_per_block, u8 *workspace)
{
    u32 block_sectors = pages_per_block * SECTORS_PER_PAGE;
    while (count) {
        u32 offset = sector & (block_sectors - 1);
        u32 batch = block_sectors - offset;
        if (batch > count) batch = count;
        if (!rewrite_block((sector - offset) / SECTORS_PER_PAGE, pages_per_block,
                           offset, batch, buffer, workspace)) return false;
        sector += batch;
        count -= batch;
        buffer += batch * SECTOR_SIZE;
    }
    return true;
}
