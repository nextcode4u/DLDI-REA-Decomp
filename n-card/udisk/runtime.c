#include "runtime.h"

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* Installed runtime ARM9 0x0200388C. Dispatch is separated from endpoint
   control and command side effects. */
enum udisk_usb_command udisk_usb_decode_wrapper(const uint8_t *packet, size_t length,
                                                struct udisk_usb_wrapper *wrapper)
{
    if (length != 31 || read_le32(packet) != UINT32_C(0x43425355))
        return UDISK_USB_BAD_WRAPPER;

    wrapper->tag = read_le32(packet + 4);
    wrapper->transfer_bytes = read_le32(packet + 8);
    wrapper->flags = packet[12];
    if (packet[13] != 0 || packet[14] == 0 || packet[14] > 16)
        return UDISK_USB_UNSUPPORTED;

    switch (packet[15]) {
    case 0x00: return UDISK_USB_TEST_UNIT_READY;
    case 0x03: return UDISK_USB_REQUEST_SENSE;
    case 0x12: return UDISK_USB_INQUIRY;
    case 0x1a: return UDISK_USB_MODE_SENSE_6;
    case 0x23: return UDISK_USB_READ_FORMAT_CAPACITIES;
    case 0x25: return UDISK_USB_READ_CAPACITY;
    case 0x28: return UDISK_USB_READ_10;
    case 0x2a: return UDISK_USB_WRITE_10;
    case 0x2f: return UDISK_USB_VERIFY_10;
    case 0x5a: return UDISK_USB_MODE_SENSE_10;
    default: return UDISK_USB_UNSUPPORTED;
    }
}

/* ARM9 0x020037C4. The original uses only the low 24 bits of the CDB LBA. */
bool udisk_usb_rw10_range(const uint8_t packet[31], uint32_t *byte_offset, uint32_t *byte_count)
{
    uint32_t lba = ((uint32_t)packet[18] << 16) | ((uint32_t)packet[19] << 8) | packet[20];
    uint32_t sectors = ((uint32_t)packet[22] << 8) | packet[23];
    *byte_offset = lba * 512;
    *byte_count = sectors * 512;
    return *byte_count == read_le32(packet + 8);
}

static void write_le(uint8_t *dest, uint32_t value, size_t length)
{
    for (size_t byte = 0; byte < length; ++byte)
        dest[byte] = (uint8_t)(value >> (byte * 8));
}

/* The boot sector assembled by ARM9 0x02000E54. The NAND operations and
   subsequent reads of the new filesystem are outside this function. */
bool udisk_build_volume_boot_sector(uint32_t sectors, uint8_t boot_sector[512])
{
    if (sectors < 0x100 || sectors > 0x7fffff)
        return false;

    uint32_t entries = ((sectors - 0x100) >> 7) + 2;
    bool fat16 = entries > 0xff7;
    uint32_t bytes = fat16 ? entries * 2 : (entries * 3 + 1) / 2;
    entries -= (bytes + 511) / 512;
    bytes = fat16 ? entries * 2 : (entries * 3 + 1) / 2;
    uint32_t fat_sectors = (bytes + 511) / 512;
    uint32_t reserved = 0x80 - (fat_sectors * 2 & 0x7f);

    for (size_t byte = 0; byte < 512; ++byte)
        boot_sector[byte] = 0;
    boot_sector[0] = 0xeb;
    boot_sector[1] = 0x3c;
    boot_sector[2] = 0x90;
    const char oem[] = "DSGBAFLS";
    const char label[] = "FLASHFORMAT";
    const char format[] = "FAT1?   ";
    for (size_t byte = 0; byte < sizeof(oem) - 1; ++byte)
        boot_sector[3 + byte] = (uint8_t)oem[byte];
    write_le(boot_sector + 11, 512, 2);
    boot_sector[13] = 0x80;
    write_le(boot_sector + 14, reserved, 2);
    boot_sector[16] = 2;
    write_le(boot_sector + 17, 2048, 2);
    if (sectors * 512 > 0x2000000)
        write_le(boot_sector + 32, sectors, 4);
    else
        write_le(boot_sector + 19, sectors, 2);
    boot_sector[21] = 0xf8;
    write_le(boot_sector + 22, fat_sectors, 2);
    boot_sector[24] = 0x20;
    boot_sector[26] = 2;
    boot_sector[38] = 0x29;
    boot_sector[39] = 0x91;
    for (size_t byte = 0; byte < sizeof(label) - 1; ++byte)
        boot_sector[43 + byte] = (uint8_t)label[byte];
    for (size_t byte = 0; byte < sizeof(format) - 1; ++byte)
        boot_sector[54 + byte] = (uint8_t)format[byte];
    boot_sector[58] = fat16 ? '6' : '2';
    boot_sector[510] = 0x55;
    boot_sector[511] = 0xaa;
    return true;
}

/* Installed runtime ARM9 0x020008E4. File lookup and cluster translation
   use the mounted FAT volume. The resulting map is kept in host memory. */
enum udisk_launch_result udisk_build_launch_map(const struct udisk_launch_io *io,
                                                uint32_t sectors_per_cluster,
                                                uint32_t root_sector, uint32_t data_sector,
                                                const char **selected_path,
                                                uint32_t *map, size_t map_capacity,
                                                size_t *map_entries)
{
    static const char *const paths[] = {
        "/autorun1.nds", "/autorun2.nds", "/autorun3.nds", "/xmenu.dat",
    };
    if (!io || !io->find_file || !io->next_cluster || !io->physical_address ||
        !selected_path || !map || !map_entries || sectors_per_cluster < 32)
        return UDISK_LAUNCH_INVALID_INPUT;

    *map_entries = 0;
    *selected_path = NULL;
    uint32_t cluster = 0;
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (io->find_file(io->context, paths[i], &cluster)) {
            *selected_path = paths[i];
            break;
        }
    }
    if (!*selected_path)
        return UDISK_LAUNCH_FILE_MISSING;

    size_t entries_per_cluster = sectors_per_cluster / 32;
    for (;;) {
        if (map_capacity - *map_entries < entries_per_cluster)
            return UDISK_LAUNCH_MAP_FULL;

        uint32_t sector = cluster < 2 ? root_sector : data_sector + (cluster - 2) * sectors_per_cluster;
        uint32_t logical = sector * 512;
        for (size_t i = 0; i < entries_per_cluster; ++i) {
            uint32_t physical = io->physical_address(io->context, logical);
            if (!*map_entries && physical % 0x4000)
                return UDISK_LAUNCH_UNALIGNED;
            map[(*map_entries)++] = physical;
            logical += 0x4000;
        }

        cluster = io->next_cluster(io->context, cluster);
        if (cluster == UINT32_C(0x0FFFFFFF))
            return UDISK_LAUNCH_READY;
    }
}
