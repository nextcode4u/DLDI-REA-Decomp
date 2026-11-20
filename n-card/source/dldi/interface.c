#include "../nrio.h"

extern void nrio_ftl_write(u32 offset, const void *buffer, u32 size);
extern void nrio_ftl_read(u32 offset, void *buffer, u32 size);
extern void nrio_ftl_init(u32 mode);

enum { SECTOR_SHIFT = 9 };

NRIO_ROUTINE(1334) bool nrio_is_inserted(void)
{
    return true;
}

NRIO_ROUTINE(133c) bool nrio_clear_status(void)
{
    return true;
}

NRIO_ROUTINE(1344) bool nrio_shutdown(void)
{
    return true;
}

NRIO_ROUTINE(134c) bool nrio_write_sectors(u32 sector, u32 count, const void *buffer)
{
    nrio_ftl_write(sector << SECTOR_SHIFT, buffer, count << SECTOR_SHIFT);
    return true;
}

NRIO_ROUTINE(1378) bool nrio_read_sectors(u32 sector, u32 count, void *buffer)
{
    nrio_ftl_read(sector << SECTOR_SHIFT, buffer, count << SECTOR_SHIFT);
    return true;
}

NRIO_ROUTINE(13a4) bool nrio_startup(void)
{
    nrio_ftl_init(0);
    return true;
}
