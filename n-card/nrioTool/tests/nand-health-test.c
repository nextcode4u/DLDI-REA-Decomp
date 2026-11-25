/* Simulated NAND. No erase or program callback is available to the scanner. */
#include "../nand-health.h"
#include "udisk/updater.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { PAGE = 2048, SECTOR = 512 };
struct cart {
    unsigned pages, retries, stop;
    uint32_t base, bit_page;
    unsigned events[9];
    bool broken_bitmap, no_bitmap;
};
static void tag(uint8_t *spare, uint8_t value)
{
    spare[0] = spare[2] = 0x55;
    spare[1] = spare[3] = 0xaa;
    spare[7] = spare[11] = spare[15] = value;
}
static unsigned data_block(const struct cart *c, uint32_t page)
{
    return page >= c->base ? (page - c->base) / c->pages : 99;
}
static bool read_spare(void *ctx, uint32_t page, uint8_t spare[64])
{
    struct cart *c = ctx;
    unsigned block = data_block(c, page);
    if (!c->no_bitmap && (page == c->bit_page || page == c->bit_page + 1)) {
        if (c->broken_bitmap) return false;
        memset(spare, 0xff, 64);
        tag(spare, 0xaa);
        return true;
    }
    if (block == 1 && !c->no_bitmap && !c->broken_bitmap)
        assert(!"known bad data block should not be probed");
    if (block == 2) return false;
    if (block == 3 && !c->retries++) return false;
    memset(spare, 0xff, 64);
    /* Arbitrary boot data is not classified as a bad marker. */
    if (page == 0 || block == 4) spare[0] = 0;
    if (block == 5 || block == 6) {
        tag(spare, 0x00);
        uint8_t data[512] = {0};
        uint8_t ecc[3];
        data[0] = 0x5a;
        udisk_payload_ecc(data, ecc);
        if (block == 5) ecc[0] ^= 1;
        for (unsigned i = 0; i < 3; ++i) memcpy(spare + 4 + i * 4, ecc, 3);
    }
    return true;
}
static bool read_main(void *ctx, uint32_t page, uint8_t main[PAGE])
{
    struct cart *c = ctx;
    /* A page within block 0, and a page only hit by a manual full recheck. */
    if (page == 17 || page == c->base + 7 * c->pages + 9) return false;
    memset(main, 0, PAGE);
    if (!c->no_bitmap && page == c->bit_page) main[0] = 2;
    if (data_block(c, page) == 5 || data_block(c, page) == 6) main[0] = 0x5a;
    return true;
}
static void event(void *ctx, uint32_t address, enum nrio_health_area area, unsigned flags)
{
    struct cart *c = ctx;
    if (area == NRIO_HEALTH_DATA) c->events[data_block(c, address / PAGE)] = flags;
    if (area == NRIO_HEALTH_BOOT && !address) c->events[8] = flags;
}
static bool progress(void *ctx, uint32_t done, uint32_t total)
{
    struct cart *c = ctx;
    assert(done < total);
    return !c->stop || done < c->stop;
}
int main(void)
{
    for (unsigned pages = 64; pages <= 128; pages *= 2) {
        const uint32_t base = (pages == 64 ? 0x800000 : 0x1000000) / PAGE;
        const uint32_t sectors = (base + 8 * pages) * PAGE / SECTOR;
        struct cart c = {.pages = pages, .base = base, .bit_page = base - 0x200000 / PAGE};
        struct nrio_health_io io = {&c, read_spare, read_main, event, progress};
        struct nrio_health_report r;
        assert(nrio_scan_nand_health(&io, sectors, pages, &r));
        assert(r.scanned == r.total && r.bitmap == NRIO_BITMAP_FOUND && r.listed == 1);
        assert(r.boot_issues == 1 && r.data_issues == 4);
        assert(c.events[8] == NRIO_HEALTH_IO);
        assert(c.events[1] == NRIO_HEALTH_LISTED && c.events[2] == NRIO_HEALTH_IO);
        assert(c.events[3] == NRIO_HEALTH_RETRIED && c.events[4] == NRIO_HEALTH_SPARE);
        assert(c.events[5] == NRIO_HEALTH_ECC && !c.events[6] && !c.events[7]);

        unsigned flags;
        assert(nrio_recheck_nand_block(&io, sectors, pages, (base + 7 * pages) * PAGE,
                                       NRIO_HEALTH_DATA, &flags) && flags == NRIO_HEALTH_IO);
        assert(!nrio_recheck_nand_block(&io, sectors, pages, 2, NRIO_HEALTH_DATA, &flags));
        c.stop = 4;
        assert(nrio_scan_nand_health(&io, sectors, pages, &r) && r.cancelled && r.scanned == 4);
        c.stop = 0;
        c.broken_bitmap = true;
        assert(nrio_scan_nand_health(&io, sectors, pages, &r) &&
               r.bitmap == NRIO_BITMAP_READ_ERROR && !r.listed);
        c.no_bitmap = true;
        assert(nrio_scan_nand_health(&io, sectors, pages, &r) &&
               r.bitmap == NRIO_BITMAP_ABSENT && !r.listed);
    }
    puts("NAND health bitmap, offsets, ECC, retries, cancellation and full-block recheck passed.");
}
