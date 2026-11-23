#ifndef NRIO_MOUNT_VOLUME_H
#define NRIO_MOUNT_VOLUME_H

#include <stdbool.h>
#include <stdint.h>

enum { NRIO_USB_SECTOR_BYTES = 512, NRIO_USB_MAX_SECTORS = 0x400000 };

uint32_t nrio_usb_fat_capacity(const uint8_t sector[NRIO_USB_SECTOR_BYTES]);
bool nrio_usb_fat_partition(const uint8_t sector[NRIO_USB_SECTOR_BYTES],
                            uint32_t *first, uint32_t *count);
bool nrio_usb_transfer_fits(uint32_t capacity, uint32_t lba, uint32_t offset, uint32_t size);

#endif
