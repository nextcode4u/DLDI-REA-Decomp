#include "raw_nand.h"

static struct nand_geometry geometry;

bool nrio_raw_startup(void)
{
    return nand_probe(&geometry);
}

bool nrio_raw_inserted(void)
{
    return geometry.sectors != 0;
}

bool nrio_raw_read(u32 sector, u32 count, void *buffer)
{
    return buffer && sector < geometry.sectors && count <= geometry.sectors - sector &&
           nand_read_main(sector, count, buffer);
}

bool nrio_raw_write(u32 sector, u32 count, const void *buffer)
{
    (void)sector;
    (void)count;
    (void)buffer;
    return false;
}

bool nrio_raw_clear(void)
{
    return true;
}

bool nrio_raw_shutdown(void)
{
    geometry.sectors = 0;
    return true;
}
