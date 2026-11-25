#include "../nand-capacity-probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum behavior { REAL_UPPER, MIRROR, BLANK, UNSTABLE, NO_UPPER_READ };
struct test { uint32_t half, upper_reads; unsigned samples, mode; };

static bool read(void *opaque, uint32_t row, uint8_t data[512])
{
    struct test *t = opaque;
    bool upper = row >= t->half;
    assert(row < 2 * t->half);
    if (upper && t->mode == NO_UPPER_READ) return false;
    if (upper && t->mode == BLANK) { memset(data, 0xff, 512); return true; }
    memset(data, 0, 512);
    uint32_t encoded = t->mode == MIRROR && upper ? row - t->half : row;
    memcpy(data, &encoded, sizeof(encoded));
    data[20] = 0xab;
    if (t->mode == UNSTABLE && upper) data[30] = ++t->upper_reads;
    return true;
}

static void sample(void *opaque, uint32_t low, uint32_t high, enum nrio_probe_observation result)
{
    struct test *t = opaque;
    assert((low ^ high) == UINT32_C(0x80000000));
    assert(low % (UINT32_C(0x80000000) / t->half) == 0);
    assert(result < NRIO_PROBE_OBSERVATIONS);
    ++t->samples;
}

int main(void)
{
    static const enum nrio_probe_observation expected[] = {
        NRIO_PROBE_DISTINCT, NRIO_PROBE_IDENTICAL, NRIO_PROBE_BLANK,
        NRIO_PROBE_UNSTABLE, NRIO_PROBE_READ_ERROR,
    };
    for (unsigned bytes = 2048; bytes <= 16384; bytes *= 2) {
        for (unsigned mode = REAL_UPPER; mode <= NO_UPPER_READ; ++mode) {
            struct test t = {.half = UINT32_C(0x80000000) / bytes, .mode = mode};
            struct nrio_capacity_probe_io io = {&t, read, sample};
            struct nrio_capacity_report r;
            assert(nrio_probe_4gib(&io, bytes, &r));
            assert(r.sampled == 12 && t.samples == 12 && r.count[expected[mode]] == 12);
            assert(!nrio_probe_4gib(&io, 512, &r));
        }
    }
    puts("4 GiB probe tested with distinct, mirrored, blank, unstable and failing upper reads.");
}
