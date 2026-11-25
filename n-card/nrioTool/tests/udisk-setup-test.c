/* Host checks with the pre-upgrade main NAND dump and archived stock files. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../udisk-setup-io.h"
#include "../udisk-upgrade-check.h"

uint32_t nrio_update_crc32(uint32_t crc, const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (crc & 1 ? UINT32_C(0xedb88320) : 0);
    }
    return crc;
}

enum { MAP = 0x40000, RUNTIME = 0x80000, FOOTER = 0x5fff8, MAIN_BYTES = 0x180000 };

struct fixture {
    uint8_t *nand;
    uint32_t fail_read;
};

static bool read_mock(void *context, uint32_t lba, uint8_t output[512])
{
    struct fixture *f = context;
    if (lba == f->fail_read || lba >= MAIN_BYTES / 512) return false;
    memcpy(output, f->nand + lba * 512, 512);
    return true;
}

static void read_bytes(const char *path, void *buffer, size_t count)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fread(buffer, 1, count, file) == count);
    assert(fclose(file) == 0);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s pre-upgrade-main.bin stock-udisk.nds stock-update.nds\n", argv[0]);
        return 2;
    }

    struct fixture f = {malloc(MAIN_BYTES), UINT32_MAX};
    assert(f.nand);
    read_bytes(argv[1], f.nand, MAIN_BYTES);
    const uint32_t capacity = 4194304;

    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_LEGACY_123);
    f.nand[MAP + 0x1234] ^= 1;
    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_UNKNOWN);
    f.nand[MAP + 0x1234] ^= 1;
    f.nand[RUNTIME + 0x8000] ^= 1;
    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_UNKNOWN);
    f.nand[RUNTIME + 0x8000] ^= 1;
    f.fail_read = MAP / 512 + 20;
    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_READ_ERROR);
    f.fail_read = UINT32_MAX;

    read_bytes(argv[2], f.nand + RUNTIME, 512);
    f.nand[FOOTER] = 3;
    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_STOCK_145);
    f.nand[FOOTER] = 2;
    assert(nrio_udisk_classify(read_mock, &f, capacity) == NRIO_UDISK_LAYOUT_UNKNOWN);
    free(f.nand);

    const struct nrio_file_id runtime = {0xc0000, 0x3dbef02e};
    const struct nrio_file_id updater = {0xe5400, 0x1994e314};
    assert(nrio_file_matches(argv[2], runtime));
    assert(nrio_file_matches(argv[3], updater));
    assert(!nrio_file_matches(argv[2], updater));

    char directory[] = "/tmp/nrio-setup-test.XXXXXX";
    assert(mkdtemp(directory));
    char destination[96], temporary[96];
    snprintf(destination, sizeof(destination), "%s/autorun1.nds", directory);
    snprintf(temporary, sizeof(temporary), "%s/autorun.part", directory);
    assert(nrio_copy_new_file(argv[2], destination, temporary, updater) == NRIO_FILE_ERROR);
    assert(access(temporary, F_OK) && access(destination, F_OK));

    const char original[] = "existing user file";
    FILE *conflict = fopen(destination, "wb");
    assert(conflict && fputs(original, conflict) >= 0 && fclose(conflict) == 0);
    assert(nrio_copy_new_file(argv[3], destination, temporary, updater) == NRIO_FILE_EXISTS);
    char remains[sizeof(original)] = {0};
    read_bytes(destination, remains, sizeof(original) - 1);
    assert(!strcmp(remains, original));
    assert(remove(destination) == 0);

    assert(nrio_copy_new_file(argv[3], destination, temporary, updater) == NRIO_FILE_OK);
    assert(nrio_file_matches(destination, updater) && access(temporary, F_OK));
    assert(remove(destination) == 0);
    assert(rmdir(directory) == 0);

    puts("Legacy/stock detection, mismatch/read failure, updater staging and no-overwrite checks passed.");
    return 0;
}
