#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "udisk-upgrade-check.h"
#include "srl-dump.h"

enum { SECTOR = 512, NAND_BYTES = 0x300000, FIRST = 0x80000 };
static uint8_t nand[NAND_BYTES];
static uint32_t addresses[NRIO_SRL_MAP_ENTRIES];

uint32_t nrio_update_crc32(uint32_t crc, const uint8_t *data, size_t size)
{
    while (size--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (crc & 1 ? 0xedb88320u : 0);
    }
    return crc;
}

bool nrio_recovery_read(uint32_t lba, uint32_t count, void *data)
{
    if (lba >= NAND_BYTES / SECTOR || count > NAND_BYTES / SECTOR - lba) return false;
    memcpy(data, nand + lba * SECTOR, count * SECTOR);
    return true;
}

static bool read_sector(void *context, uint32_t lba, uint8_t data[SECTOR])
{
    (void)context;
    return nrio_recovery_read(lba, 1, data);
}

static void le32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    char *label = argv[2];
    unsigned bytes = (unsigned)strtoul(argv[3], NULL, 16), blocks;
    const uint32_t map = (uint32_t)strtoul(argv[4], NULL, 16);
    size_t total = 0;
    memset(nand, 0xff, sizeof(nand));
    for (unsigned i = 0; i < 72; ++i) {
        unsigned offset = FIRST + i * 2 * NRIO_SRL_MAP_BLOCK;
        le32(nand + map + i * 4, offset);
        total += fread(nand + offset, 1, NRIO_SRL_MAP_BLOCK, file);
    }
    assert(total >= bytes && !ferror(file) && fclose(file) == 0);
    const struct nrio_udisk_archive *archive = nrio_identify_archived_udisk(
        read_sector, NULL, NAND_BYTES / SECTOR, map);
    assert(archive && !strcmp(archive->name, label) && archive->bytes == bytes);
    assert(nrio_srl_plan(map, NAND_BYTES / SECTOR, addresses, &blocks));
    assert(blocks == (bytes + NRIO_SRL_MAP_BLOCK - 1) / NRIO_SRL_MAP_BLOCK);
    if (archive->wrapped) {
        unsigned last = blocks - 1;
        le32(nand + map + last * 4, UINT32_MAX);
        assert(!nrio_srl_plan(map, NAND_BYTES / SECTOR, addresses, &blocks));
        le32(nand + map + last * 4, FIRST + last * 2 * NRIO_SRL_MAP_BLOCK);
    }

    /* The second sampled sector is mapped non-contiguously. A corrupt
       sector or an out-of-range entry must not reproduce the match. */
    nand[FIRST + 4 * NRIO_SRL_MAP_BLOCK] ^= 1;
    assert(!nrio_identify_archived_udisk(read_sector, NULL, NAND_BYTES / SECTOR, map));
    nand[FIRST + 4 * NRIO_SRL_MAP_BLOCK] ^= 1;
    le32(nand + map + 8, NAND_BYTES);
    assert(!nrio_identify_archived_udisk(read_sector, NULL, NAND_BYTES / SECTOR, map));
    le32(nand + map + 8, FIRST + 4 * NRIO_SRL_MAP_BLOCK);
    memcpy(nand + map - 4, "QAFE", 4);
    assert(!nrio_identify_archived_udisk(read_sector, NULL, NAND_BYTES / SECTOR, map));

    /* Read-only archive recognition must not loosen the upgrade gate. */
    assert(nrio_udisk_classify(read_sector, NULL, NAND_BYTES / SECTOR) == NRIO_UDISK_LAYOUT_UNKNOWN);
    printf("%s passed on map 0x%X.\n", label, map);
}
