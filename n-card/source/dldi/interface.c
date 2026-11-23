#include "../nrio.h"

extern void nrio_ftl_write(u32 offset, const void *buffer, u32 size);
extern void nrio_ftl_read(u32 offset, u32 buffer_address, u32 size);
extern void nrio_ftl_init(u32 mode);

enum { SECTOR_SHIFT = 9 };

#ifdef NRIO_USB_MOUNT
extern u32 nrio_sector_capacity;

static bool valid_transfer(u32 sector, u32 count, const void *buffer)
{
    return buffer && count && sector < nrio_sector_capacity &&
           count <= nrio_sector_capacity - sector;
}
#endif

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
#ifdef NRIO_USB_MOUNT
    if (!valid_transfer(sector, count, buffer)) return false;
#endif
    nrio_ftl_write(sector << SECTOR_SHIFT, buffer, count << SECTOR_SHIFT);
    return true;
}

NRIO_ROUTINE(1378) bool nrio_read_sectors(u32 sector, u32 count, void *buffer)
{
#ifdef NRIO_USB_MOUNT
    if (!valid_transfer(sector, count, buffer)) return false;
#endif
    nrio_ftl_read(sector << SECTOR_SHIFT, (u32)buffer, count << SECTOR_SHIFT);
    return true;
}

NRIO_ROUTINE(13a4) bool nrio_startup(void)
{
    nrio_ftl_init(0);
    return true;
}
