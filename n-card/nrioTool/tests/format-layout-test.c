/* Host test of the console formatter's policy and real map-range checks.
   Compile with quick-format.c and srl-dump.c in the applied checkout, with
   host nds.h shims for consoleClear and the libnds integer types. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "region-recovery.h"
#include "srl-dump.h"
#include "udisk-setup.h"
#include "udisk-upgrade-check.h"
#include "udisk/format-native.h"
#include "recovery_nand/raw_nand.h"

static uint32_t pages, sectors, second_chunk;
static enum nrio_boot_kind primary, secondary;
static enum nrio_udisk_layout legacy;
static bool change_at_confirm;
static unsigned confirms, formats;

static void put32(uint8_t *data, uint32_t val)
{
    for (unsigned i = 0; i < 4; ++i) data[i] = val >> (8 * i);
}

bool nrio_recovery_startup(void) { return true; }
void nrio_recovery_shutdown(void) {}
bool nrio_recovery_select_mode(enum nrio_raw_mode mode) { return mode == NRIO_RAW_MAIN_READ; }
bool nrio_recovery_get_info(struct nrio_raw_info *info)
{
    *info = (struct nrio_raw_info){.nand_id = pages == 128 ? 0x2514d3ec : 0x0514d3ec, .sectors = sectors};
    return true;
}
uint32_t nrio_recovery_pages_per_block(void) { return pages; }
bool nrio_recovery_read(uint32_t lba, uint32_t count, void *buf)
{
    uint8_t *data = buf;
    assert(count == 1);
    memset(data, 0, 512);
    if (lba == 2) {
        memcpy(data, "DSGB", 4);
        memcpy(data + 0xc, "DSGB", 4);
    } else if (lba == NRIO_RAW_PRIMARY_TABLE / 512) {
        put32(data, 0x20000);
    } else if (lba == NRIO_RAW_SECONDARY_TABLE / 512) {
        for (unsigned i = 0; i < 48; ++i) put32(data + i * 4, 0x80000 + i * 0x4000);
        put32(data + 4, second_chunk);
    }
    return true;
}

struct nrio_boot_rom nrio_inspect_boot(uint32_t table, uint32_t capacity)
{
    assert(capacity == sectors);
    if (table == NRIO_RAW_PRIMARY_TABLE)
        return (struct nrio_boot_rom){.kind = primary, .address = 0x20000, .size = 0x4000};
    return (struct nrio_boot_rom){.kind = secondary, .address = 0x80000, .size = 0x8000};
}
const char *nrio_boot_kind_name(enum nrio_boot_kind kind) { (void)kind; return "Test ROM"; }
enum nrio_udisk_layout nrio_udisk_classify(nrio_read_nand_sector read, void *opaque, uint32_t capacity)
{
    (void)read; (void)opaque; (void)capacity;
    return legacy;
}

bool nrio_udisk_confirm(void)
{
    ++confirms;
    if (change_at_confirm) second_chunk += 0x4000;
    return true;
}
void nrio_udisk_wait_back(void) {}

bool udisk_native_quick_format(const struct udisk_native_format_io *io, uint32_t capacity,
                              uint32_t block_pages, struct udisk_native_format_report *out)
{
    assert(io && capacity == sectors && block_pages == pages);
    *out = (struct udisk_native_format_report){0};
    ++formats;
    return true;
}
bool udisk_native_full_format(const struct udisk_native_format_io *io, uint32_t capacity,
                             uint32_t block_pages, struct udisk_native_format_report *out)
{
    return udisk_native_quick_format(io, capacity, block_pages, out);
}
bool nand_load_page(u32 p, bool spare) { (void)p; (void)spare; return false; }
bool nand_read_sector(u32 col, u8 *out) { (void)col; (void)out; return false; }
bool nand_read_spare(u8 *out) { (void)out; return false; }
bool nand_erase_block(u32 page) { (void)page; assert(0); return false; }
bool nand_program_page(u32 page, const u8 *data) { (void)page; (void)data; assert(0); return false; }

int main(void)
{
    pages = 128; sectors = 0x200000;
    primary = NRIO_BOOT_PATCHED; secondary = NRIO_BOOT_XULU; second_chunk = 0x84000;
    nrioTool_quick_format(NULL);
    assert(confirms == 1 && formats == 1);

    pages = 64;
    nrioTool_full_format(NULL);
    assert(confirms == 2 && formats == 2);

    primary = NRIO_BOOT_XULU; secondary = NRIO_BOOT_ERASED;
    nrioTool_quick_format(NULL);
    assert(confirms == 3 && formats == 3);

    primary = NRIO_BOOT_STOCK; secondary = NRIO_BOOT_UNKNOWN;
    nrioTool_quick_format(NULL);
    assert(confirms == 3 && formats == 3);

    legacy = NRIO_UDISK_LAYOUT_LEGACY_123;
    nrioTool_quick_format(NULL);
    assert(confirms == 4 && formats == 4);

    secondary = NRIO_BOOT_XULU;
    second_chunk = 0x600000;
    nrioTool_quick_format(NULL);
    assert(confirms == 4 && formats == 4);

    second_chunk = 0x84000;
    change_at_confirm = true;
    nrioTool_quick_format(NULL);
    assert(confirms == 5 && formats == 4);

    puts("Quick Format route, protection and recheck tests passed.");
}
