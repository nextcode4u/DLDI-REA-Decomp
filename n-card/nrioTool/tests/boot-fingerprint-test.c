#include "../udisk-upgrade-check.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct mock { unsigned calls; bool unstable, fail; uint8_t fill; };

static bool read_sector(void *opaque, uint32_t lba, uint8_t data[512])
{
    struct mock *m = opaque;
    ++m->calls;
    if (m->fail && m->calls == 3) return false;
    memset(data, m->fill, 512);
    if (m->unstable && m->calls == 2) data[511] ^= 1;
    if (m->fill == 0x39) data[0] = (uint8_t)lba;
    return true;
}

uint32_t nrio_update_crc32(uint32_t crc, const uint8_t *data, size_t bytes)
{
    while (bytes--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (crc & 1 ? UINT32_C(0xedb88320) : 0);
    }
    return crc;
}

int main(void)
{
    struct mock m = {.fill = 0x39};
    uint32_t crc;
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 512, 1024, &crc) == NRIO_BOOT_HASH_OK);
    assert(m.calls == 4 && crc == UINT32_C(0xf4d0389b));
    m = (struct mock){.unstable = true};
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 512, 1024, &crc) == NRIO_BOOT_HASH_UNSTABLE);
    assert(m.calls == 2 && !crc);
    m = (struct mock){.fail = true};
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 512, 1024, &crc) == NRIO_BOOT_HASH_READ_ERROR);
    assert(!crc);
    m = (struct mock){.fill = 0xff};
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 512, 1024, &crc) == NRIO_BOOT_HASH_ERASED);
    m.fill = 0;
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 512, 1024, &crc) == NRIO_BOOT_HASH_ZERO);
    m.calls = 0;
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 1536, 1024, &crc) == NRIO_BOOT_HASH_UNAVAILABLE);
    assert(nrio_boot_fingerprint(read_sector, &m, 4, 3, 512, &crc) == NRIO_BOOT_HASH_UNAVAILABLE);
    assert(!m.calls && !crc);
    puts("Surviving boot fingerprints reject mismatches, read failures and invalid ranges.");
}
