#include "../nrio.h"

#ifdef NRIO_UNIFIED
extern bool nrio_ftl_write(u32 offset, const void *buffer, u32 size);
#else
extern void nrio_ftl_write(u32 offset, const void *buffer, u32 size);
#endif
extern void nrio_ftl_read(u32 offset, u32 buffer_address, u32 size);
#ifdef NRIO_EXPERIMENTAL
#include "../ftl/geometry.h"
extern bool nrio_ftl_init(u32 mode);
#else
extern void nrio_ftl_init(u32 mode);
#endif

enum { SECTOR_SHIFT = 9 };

#ifdef NRIO_USB_MOUNT
extern u32 nrio_sector_capacity;

#ifndef NRIO_EXPERIMENTAL
static bool valid_transfer(u32 sector, u32 count, const void *buffer)
{
    return buffer && count && sector < nrio_sector_capacity &&
           count <= nrio_sector_capacity - sector;
}
#endif
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
#ifdef NRIO_EXPERIMENTAL
    nrio_sector_capacity = 0;
#endif
    return true;
}

NRIO_ROUTINE(134c) bool nrio_write_sectors(u32 sector, u32 count, const void *buffer)
{
#if defined(NRIO_UNIFIED)
    u32 offset, bytes;
    return buffer && nrio_byte_range(sector, count, nrio_sector_capacity, &offset, &bytes) &&
           nrio_ftl_write(offset, buffer, bytes);
#elif defined(NRIO_EXPERIMENTAL)
    (void)sector; (void)count; (void)buffer;
    return false;
#else
#ifdef NRIO_USB_MOUNT
    if (!valid_transfer(sector, count, buffer)) return false;
#endif
    nrio_ftl_write(sector << SECTOR_SHIFT, buffer, count << SECTOR_SHIFT);
    return true;
#endif
}

NRIO_ROUTINE(1378) bool nrio_read_sectors(u32 sector, u32 count, void *buffer)
{
#if defined(NRIO_EXPERIMENTAL)
    u32 offset, bytes;
    if (!buffer || !nrio_byte_range(sector, count, nrio_sector_capacity, &offset, &bytes)) return false;
    if ((u32)buffer & 3) {
        u32 aligned[128];
        u8 *dest = buffer;
        for (u32 i = 0; i < count; ++i) {
            nrio_ftl_read(offset, (u32)aligned, 512);
#ifdef NRIO_UNIFIED
            if (!nrio_sector_capacity) return false;
#endif
            for (unsigned b = 0; b < 512; ++b) *dest++ = ((const u8 *)aligned)[b];
            offset += 512;
        }
    } else nrio_ftl_read(offset, (u32)buffer, bytes);
#ifdef NRIO_UNIFIED
    return nrio_sector_capacity != 0;
#else
    return true;
#endif
#else
#ifdef NRIO_USB_MOUNT
    if (!valid_transfer(sector, count, buffer)) return false;
#endif
    nrio_ftl_read(sector << SECTOR_SHIFT, (u32)buffer, count << SECTOR_SHIFT);
    return true;
#endif
}

NRIO_ROUTINE(13a4) bool nrio_startup(void)
{
#ifdef NRIO_EXPERIMENTAL
    return nrio_ftl_init(0);
#else
    nrio_ftl_init(0);
    return true;
#endif
}
