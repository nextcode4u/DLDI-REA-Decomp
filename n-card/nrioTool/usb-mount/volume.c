#include "volume.h"

#include <string.h>

static uint32_t le16(const uint8_t *data)
{
    return data[0] | (uint32_t)data[1] << 8;
}

static uint32_t le32(const uint8_t *data)
{
    return le16(data) | le16(data + 2) << 16;
}

static bool boot_signature(const uint8_t *sector)
{
    return sector[510] == 0x55 && sector[511] == 0xaa;
}

uint32_t nrio_usb_fat_capacity(const uint8_t sector[NRIO_USB_SECTOR_BYTES])
{
    if (!boot_signature(sector) || (sector[0] != 0xe9 && sector[0] != 0xeb) ||
        le16(sector + 11) != NRIO_USB_SECTOR_BYTES)
        return 0;

    uint32_t total = le16(sector + 19);
    if (!total) total = le32(sector + 32);

    uint32_t cluster = sector[13], reserved = le16(sector + 14), fats = sector[16];
    uint32_t roots = le16(sector + 17), fat_sectors = le16(sector + 22);
    if (!fat_sectors) fat_sectors = le32(sector + 36);
    if (!total || total > NRIO_USB_MAX_SECTORS || !cluster || cluster > 128 ||
        (cluster & (cluster - 1)) || !reserved || !fats || fats > 2 ||
        !fat_sectors || fat_sectors >= total)
        return 0;
    if (reserved + fats * fat_sectors + (roots + 15) / 16 >= total)
        return 0;

    const uint8_t *label = sector + (le16(sector + 22) ? 54 : 82);
    return !memcmp(label, "FAT", 3) || !memcmp(sector + 3, "DSGBAFLS", 8) ? total : 0;
}

bool nrio_usb_fat_partition(const uint8_t sector[NRIO_USB_SECTOR_BYTES],
                            uint32_t *first, uint32_t *count)
{
    if (!boot_signature(sector)) return false;

    const uint8_t *partition = NULL;
    for (unsigned i = 0; i < 4; ++i) {
        const uint8_t *entry = sector + 0x1be + i * 16;
        if (!entry[4]) continue;
        if (partition || (entry[0] != 0 && entry[0] != 0x80)) return false;
        switch (entry[4]) {
            case 0x01: case 0x04: case 0x06: case 0x0e:
            case 0x0b: case 0x0c: partition = entry; break;
            default: return false;
        }
    }
    if (!partition) return false;
    *first = le32(partition + 8);
    *count = le32(partition + 12);
    return *first && *first < NRIO_USB_MAX_SECTORS && *count &&
           *count <= NRIO_USB_MAX_SECTORS - *first;
}

bool nrio_usb_transfer_fits(uint32_t capacity, uint32_t lba, uint32_t offset, uint32_t size)
{
    return !offset && size && size % NRIO_USB_SECTOR_BYTES == 0 && lba < capacity &&
           size / NRIO_USB_SECTOR_BYTES <= capacity - lba;
}
