/**
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2024 Adrian "asie" Siekierka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>
#include <nds/arm9/dldi.h>
#include "tusb.h"
#include "msc.h"
#include "ui.h"
#include "volume.h"
#include "raw_nand.h"

static const DISC_INTERFACE *media;
static uint32_t capacity;
static bool ejected;

bool msc_dldi_initialize(void)
{
    uint8_t sector[512] __attribute__((aligned(4)));
    struct nand_geometry geometry;
    const DISC_INTERFACE *io = dldiGetInternal();
    if (memcmp(&io->ioType, "NRIO", 4) ||
        strcmp(io_dldi_data->friendlyName, "N-Card filesystem USB") ||
        (io->features & (FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_NDS)) !=
        (FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_NDS))
        return false;
    sysSetCardOwner(BUS_OWNER_ARM9);
    if (!nand_probe(&geometry) || !io->startup() || !io->isInserted() ||
        !io->readSectors(0, 1, sector))
        return false;

    capacity = nrio_usb_fat_capacity(sector);
    if (!capacity) {
        uint32_t first, count;
        if (!nrio_usb_fat_partition(sector, &first, &count) ||
            first + count > geometry.sectors || !io->readSectors(first, 1, sector))
            return false;
        uint32_t filesystem = nrio_usb_fat_capacity(sector);
        if (!filesystem || filesystem > count) return false;
        capacity = first + count;
    }
    if (capacity > geometry.sectors || !io->readSectors(capacity - 1, 1, sector)) return false;
    media = io;
    ejected = false;
    printf("N-Card FAT filesystem, read/write\n%lu sectors\n", (unsigned long)capacity);
    return true;
}

bool msc_dldi_ejected(void)
{
    return ejected;
}

void msc_dldi_close(void)
{
    if (media) media->shutdown();
    media = NULL;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor[8], uint8_t product[16], uint8_t version[4])
{
    (void)lun;
    memcpy(vendor, "N-CARD  ", 8);
    memcpy(product, "N-Card FAT      ", 16);
    memcpy(version, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    if (!media || ejected || lun) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0);
        return false;
    }
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *count, uint16_t *size)
{
    *count = lun || !media || ejected ? 0 : capacity;
    *size = 512;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
    return !lun && media && !ejected;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power, bool start, bool load_eject)
{
    (void)power;
    if (lun || !media) return false;
    if (load_eject && !start && !ejected) {
        ejected = true;
        puts("Storage ejected. START to exit.");
    }
    return !ejected || !start;
}

static bool valid_io(uint8_t lun, uint32_t lba, uint32_t offset, uint32_t size)
{
    if (!tud_msc_test_unit_ready_cb(lun)) return false;
    if (!nrio_usb_transfer_fits(capacity, lba, offset, size)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0);
        return false;
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t size)
{
    if (!valid_io(lun, lba, offset, size)) return -1;
    ui_toggle_blink_activity();
    bool ok = media->readSectors(lba, size / 512, buffer);
    ui_toggle_blink_activity();
    if (ok) return size;
    tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0);
    return -1;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t size)
{
    if (!valid_io(lun, lba, offset, size)) return -1;
    ui_toggle_blink_write_activity();
    bool ok = media->writeSectors(lba, size / 512, buffer);
    ui_toggle_blink_write_activity();
    if (ok) return size;
    tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0);
    return -1;
}

int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t command[16], void *buffer, uint16_t size)
{
    (void)buffer;
    (void)size;
    /* The NRIO write callback completes its block updates synchronously. */
    if (command[0] == 0x35 && tud_msc_test_unit_ready_cb(lun)) return 0;
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0);
    return -1;
}
