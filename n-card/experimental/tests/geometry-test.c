#include "../../source/ftl/geometry.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    struct nrio_geometry g;
    assert(nrio_decode_geometry(0x2555d5ec, &g));
    assert(g.physical_sectors == 0x400000 && g.reserved_bytes == 0x1000000 &&
           g.data_blocks == 8128 && g.pages_per_block == 128);
    assert(nrio_decode_geometry(0x0514d3ec, &g));
    assert(g.physical_sectors == 0x200000 && g.reserved_bytes == 0x800000 &&
           g.data_blocks == 8128 && g.pages_per_block == 64);
    assert(!nrio_decode_geometry(0x2555d7ec, &g) &&
           !nrio_decode_geometry(0x2555deec, &g) &&
           !nrio_decode_geometry(0x2555d501, &g));

    uint32_t offset, length;
    assert(nrio_byte_range(0x3fffff, 1, 0x400000, &offset, &length) &&
           offset == 0x7ffffe00 && length == 512);
    assert(!nrio_byte_range(0x3fffff, 2, 0x400000, &offset, &length));
    assert(nrio_byte_range(0x400000, 1, 0x800000, &offset, &length) &&
           offset == 0x80000000);
    assert(nrio_byte_range(0x7fffff, 1, 0x800000, &offset, &length) &&
           offset == 0xfffffe00 && length == 512);
    assert(!nrio_byte_range(0x7fffff, 2, 0x800001, &offset, &length));
    assert(!nrio_byte_range(0, 0x800000, 0x800000, &offset, &length));
    assert(!nrio_byte_range(0, 0, 0x800000, &offset, &length));
    assert(!nrio_byte_range(UINT32_MAX, 1, UINT32_MAX, &offset, &length));
    puts("Geometry decode and range checks passed at 2 and 4 GiB.");
}
