#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../raw-nand/source/raw_nand.h"
#include "../../raw-nand/source/recovery.h"
#include "../../udisk/updater.h"

enum { IMAGE_BYTES = 0xc0000, LEGACY_BYTES = 0x180000, PAGES = 128 };
static uint8_t raw[LEGACY_BYTES / PAGE_SIZE][PAGE_RECORD_SIZE];
static uint8_t backup[PAGES][PAGE_RECORD_SIZE], work[PAGES][PAGE_RECORD_SIZE];
static uint8_t firmware[IMAGE_BYTES], map[UDISK_ALLOCATION_BYTES];
static struct nrio_sector_patch patches[PAGES * SECTORS_PER_PAGE];
static unsigned selected, erases, programmed;

bool nand_load_page(u32 page, bool spare)
{
    (void)spare;
    selected = page;
    return page < sizeof(raw) / sizeof(raw[0]);
}
bool nand_read_sector(u32 column, u8 *data)
{
    memcpy(data, raw[selected] + column * SECTOR_SIZE, SECTOR_SIZE);
    return true;
}
bool nand_read_spare(u8 *data)
{
    memcpy(data, raw[selected] + PAGE_SIZE, SPARE_SIZE);
    return true;
}
bool nand_program_page(u32 page, const u8 *data)
{
    for (unsigned i = 0; i < PAGE_RECORD_SIZE; ++i) {
        if ((raw[page][i] & data[i]) != data[i]) return false;
        raw[page][i] = data[i];
    }
    ++programmed;
    return true;
}
bool nand_erase_block(u32 page)
{
    assert(page % PAGES == 0);
    assert(page * PAGE_SIZE == 0x40000 || (page * PAGE_SIZE >= 0x80000 && page * PAGE_SIZE < 0x140000));
    memset(raw[page], 0xff, PAGES * PAGE_RECORD_SIZE);
    ++erases;
    return true;
}

static void read_exact(FILE *file, uint8_t *data, size_t bytes)
{
    assert(fread(data, 1, bytes, file) == bytes);
}

static bool run(unsigned address, const uint8_t *data, unsigned bytes, bool install,
                struct nrio_raw_write_error *error)
{
    unsigned page = address / PAGE_SIZE, count = bytes / SECTOR_SIZE;
    memcpy(backup, raw[page], sizeof(backup));
    for (unsigned i = 0; i < count; ++i)
        patches[i] = (struct nrio_sector_patch){address / SECTOR_SIZE + i, data + i * SECTOR_SIZE, NULL};
    return patch_udisk_block_snapshot(page, PAGES, work[0], backup[0], patches, count, install, error);
}

static void assert_contents(unsigned address, const uint8_t *data, unsigned bytes)
{
    for (unsigned i = 0; i < bytes / PAGE_SIZE; ++i) {
        const uint8_t *record = raw[address / PAGE_SIZE + i];
        assert(!memcmp(record, data + i * PAGE_SIZE, PAGE_SIZE));
        for (unsigned j = PAGE_SIZE; j < PAGE_RECORD_SIZE; ++j) assert(record[j] == 0xff);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fputs("Usage: native-bios-test old-main.bin stock-udisk.nds\n", stderr);
        return 2;
    }
    FILE *old = fopen(argv[1], "rb"), *image = fopen(argv[2], "rb");
    assert(old && image);
    memset(raw, 0xff, sizeof(raw));
    for (unsigned i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) read_exact(old, raw[i], PAGE_SIZE);
    read_exact(image, firmware, sizeof(firmware));
    fclose(old);
    fclose(image);

    uint32_t alloc[UDISK_ALLOCATION_COUNT];
    for (unsigned i = 0; i < UDISK_ALLOCATION_COUNT; ++i)
        alloc[i] = i < 6 ? 0x80000 + i * UDISK_ALLOCATION_BYTES : UINT32_MAX;
    udisk_build_secondary_chunk(alloc, map);

    struct nrio_raw_write_error error = {0};
    assert(!run(0, firmware, 0x40000, false, &error));
    assert(!run(0x140000, firmware, 0x40000, false, &error));
    raw[0x80000 / PAGE_SIZE][PAGE_SIZE] = 0;
    assert(!run(0x80000, firmware, 0x40000, false, &error) && error.stage == NRIO_RAW_WRITE_MARKER);
    assert(erases == 0);
    raw[0x80000 / PAGE_SIZE][PAGE_SIZE] = 0xff;

    /* Exercise the known generic-ECC compatibility case. */
    uint8_t *record = raw[0x80000 / PAGE_SIZE];
    uint8_t ecc[3];
    udisk_payload_ecc(record, ecc);
    memcpy(record + PAGE_SIZE + 4, ecc, 3);
    memcpy(record + PAGE_SIZE + 8, ecc, 3);
    memcpy(record + PAGE_SIZE + 12, ecc, 3);

    raw[0x60000 / PAGE_SIZE][PAGE_SIZE + 5] = 0xda;
    for (unsigned i = 0; i < IMAGE_BYTES; i += 0x40000)
        assert(run(0x80000 + i, firmware + i, 0x40000, false, &error));
    assert(run(0x40000, map, sizeof(map), false, &error));
    assert(erases == 0 && programmed == 0);

    for (unsigned i = 0; i < IMAGE_BYTES; i += 0x40000)
        assert(run(0x80000 + i, firmware + i, 0x40000, true, &error));
    assert_contents(0x80000, firmware, IMAGE_BYTES);
    assert(erases == 3 && raw[0x40000 / PAGE_SIZE][0] == 0);
    assert(run(0x40000, map, sizeof(map), true, &error));
    assert_contents(0x40000, map, sizeof(map));
    assert(erases == 4 && raw[0x60000 / PAGE_SIZE][PAGE_SIZE + 5] == 0xda);
    puts("Verified native BIOS page records, payload-before-map order, no preflight writes and protected spare data.");
    return 0;
}
