#ifndef NRIO_UDISK_RUNTIME_H
#define NRIO_UDISK_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum udisk_usb_command {
    UDISK_USB_BAD_WRAPPER,
    UDISK_USB_UNSUPPORTED,
    UDISK_USB_TEST_UNIT_READY,
    UDISK_USB_REQUEST_SENSE,
    UDISK_USB_INQUIRY,
    UDISK_USB_MODE_SENSE_6,
    UDISK_USB_READ_FORMAT_CAPACITIES,
    UDISK_USB_READ_CAPACITY,
    UDISK_USB_READ_10,
    UDISK_USB_WRITE_10,
    UDISK_USB_VERIFY_10,
    UDISK_USB_MODE_SENSE_10,
};

struct udisk_usb_wrapper {
    uint32_t tag, transfer_bytes;
    uint8_t flags;
};

enum udisk_usb_command udisk_usb_decode_wrapper(const uint8_t *packet, size_t length,
                                                struct udisk_usb_wrapper *wrapper);
bool udisk_usb_rw10_range(const uint8_t packet[31], uint32_t *byte_offset, uint32_t *byte_count);
bool udisk_build_volume_boot_sector(uint32_t sectors, uint8_t boot_sector[512]);

struct udisk_launch_io {
    void *context;
    bool (*find_file)(void *context, const char *path, uint32_t *first_cluster);
    uint32_t (*next_cluster)(void *context, uint32_t cluster);
    uint32_t (*physical_address)(void *context, uint32_t logical_byte_offset);
};

enum udisk_launch_result {
    UDISK_LAUNCH_READY,
    UDISK_LAUNCH_INVALID_INPUT,
    UDISK_LAUNCH_FILE_MISSING,
    UDISK_LAUNCH_UNALIGNED,
    UDISK_LAUNCH_MAP_FULL,
};

enum udisk_launch_result udisk_build_launch_map(const struct udisk_launch_io *io,
                                                uint32_t sectors_per_cluster,
                                                uint32_t root_sector, uint32_t data_sector,
                                                const char **selected_path,
                                                uint32_t *map, size_t map_capacity,
                                                size_t *map_entries);

#endif
