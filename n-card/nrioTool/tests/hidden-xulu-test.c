#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "region-recovery.h"
#include "boot-report.h"
#include "udisk-upgrade-check.h"
#include "udisk/updater.h"
#include "bundle-assets.h"
#include "recovery_nand/raw_nand.h"

static u8 main_data[0x200000], spare_data[0x200000 / PAGE_SIZE * SPARE_SIZE];
static u8 original_main[sizeof(main_data)], original_spare[sizeof(spare_data)];
static uint32_t erase_pages[16], erase_count, row, pages = 128;
static bool permit, change_on_confirm, fail_secondary_read, corrupt_backup;
static unsigned confirm_count, programs;
static uint32_t workspace[NRIO_RAW_WORKSPACE_BYTES / 4];
static u8 snapshot[NRIO_RAW_WORKSPACE_BYTES];
void nrio_region_screen(const char *title) { puts(title); }
bool nrio_confirm_padding_erase(void) { return false; }

bool nand_probe(struct nand_geometry *geometry) {
    *geometry = (struct nand_geometry){.sectors=sizeof(main_data)/512, .id=0x2555d5ec, .pages_per_block=pages};
    return true;
}
bool nand_read_main(u32 lba, u32 count, u8 *output) {
    assert(lba+count <= sizeof(main_data)/512);
    if (fail_secondary_read && lba == 0x40000/512) return false;
    memcpy(output,main_data+lba*512,count*512);
    return true;
}
bool nand_load_page(u32 page,bool spare) { (void)spare; row=page; return row<sizeof(main_data)/2048; }
bool nand_read_sector(u32 column,u8 *output) { memcpy(output,main_data+row*2048+column*512,512); return true; }
bool nand_read_spare(u8 *output) { memcpy(output,spare_data+row*64,64); return true; }
bool nand_erase_block(u32 page) {
    assert(erase_count<16 && page%pages==0);
    erase_pages[erase_count++]=page;
    memset(main_data+page*2048,255,pages*2048);
    memset(spare_data+page*64,255,pages*64);
    return true;
}
bool nand_program_page(u32 page,const u8 *record) {
    ++programs;
    memcpy(main_data+page*2048,record,2048);
    memcpy(spare_data+page*64,record+2048,64);
    return true;
}
bool nrio_confirm_write(void) {
    ++confirm_count;
    if (change_on_confirm) main_data[0x40000+0x200] ^= 1;
    if (corrupt_backup) {
        char latest[48] = {0}, candidate[48];
        for (unsigned i=0; i<1000; ++i) {
            snprintf(candidate, sizeof(candidate), "./region-%03u/blocks.bin", i);
            if (!access(candidate, F_OK)) strcpy(latest, candidate);
        }
        assert(latest[0]);
        FILE *saved = fopen(latest, "r+b");
        assert(saved && !fseek(saved, -1, SEEK_END));
        int b = fgetc(saved);
        assert(b != EOF && !fseek(saved, -1, SEEK_END));
        assert(fputc(b ^ 1, saved) != EOF && !fclose(saved));
    }
    return permit;
}
void nrio_show_write_error(void) {
    struct nrio_raw_write_error e;
    nrio_recovery_get_write_error(&e);
    printf("Backend stage %u, page %u, detail %u\n",e.stage,e.page,e.detail);
}
uint32_t nrio_update_crc32(uint32_t crc,const u8 *data,size_t size) {
    while(size--) {
        crc^=*data++;
        for(unsigned i=0;i<8;i++) crc=(crc>>1)^(crc&1?0xedb88320u:0);
    }
    return crc;
}
static void word(u8 *p,u32 v) {
    for(unsigned i=0;i<4;i++) p[i]=(u8)(v>>(8*i));
}
static struct nrio_raw_info init_cart(const u8 *rom) {
    memset(main_data,255,sizeof(main_data));
    memset(spare_data,255,sizeof(spare_data));
    memset(spare_data,0x33,6*64);
    memcpy(main_data+0x400,"DSGB",4);
    memcpy(main_data+0x40c,"DSGB",4);
    for(unsigned i=0;i<6;i++) {
        unsigned address=0x20000+i*0x4000;
        word(main_data+0x8000+i*4,address);
        memcpy(main_data+address,rom+i*0x4000,0x4000);
    }
    memcpy(original_main,main_data,sizeof(main_data));
    memcpy(original_spare,spare_data,sizeof(spare_data));
    erase_count=programs=confirm_count=0;
    permit=true;
    change_on_confirm=fail_secondary_read=corrupt_backup=false;
    struct nrio_raw_info info;
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_SD_REGION_INSTALL));
    assert(nrio_recovery_get_info(&info) && nrio_recovery_bind_workspace(workspace,sizeof(workspace)));
    return info;
}
static void load(const char *path, u8 *buffer, size_t size) {
    FILE *f=fopen(path,"rb"); assert(f && fread(buffer,1,size,f)==size); assert(!fclose(f));
}

static struct nrio_raw_info setup(const u8 *rom) {
    struct nrio_raw_info info = init_cart(rom);
    uint32_t map[UDISK_ALLOCATION_COUNT];
    for (unsigned i=0;i<UDISK_ALLOCATION_COUNT;++i)
        map[i]=i<NRIO_ASSET_UDISK_CHUNKS ? 0x80000+i*UDISK_ALLOCATION_BYTES : UINT32_MAX;
    udisk_build_secondary_chunk(map,main_data+0x40000);
    for (unsigned i=0;i<NRIO_ASSET_UDISK_CHUNKS;++i)
        assert(nrio_asset_expand(i,main_data+map[i],UDISK_ALLOCATION_BYTES));
    memcpy(original_main,main_data,sizeof(main_data));
    return info;
}
static uint32_t read_word(const u8 *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void save(const char *name, const u8 *data, size_t size)
{
    FILE *f = fopen(name, "wb");
    assert(f && fwrite(data, 1, size, f) == size);
    assert(!fclose(f));
}

static struct nrio_raw_info setup_bootleg(const u8 *rom)
{
    struct nrio_raw_info info = init_cart(rom);
    uint32_t used = read_word(rom+0x80);
    memset(main_data+0x20000, 255, 0x20000);
    memcpy(main_data+0x40000, rom, used);
    memcpy(main_data+0x40000, "BOOTLEG FIXT", 12);
    uint16_t crc = nrio_safe_mode_crc16(main_data+0x40000, 0x15e);
    main_data[0x4015e] = (uint8_t)crc;
    main_data[0x4015f] = (uint8_t)(crc >> 8);
    memset(main_data+0x8000, 255, 0x4000);
    for (uint32_t off=0; off<used; off+=0x4000)
        word(main_data+0x8000+off/0x4000*4, 0x40000+off);
    memcpy(original_main, main_data, sizeof(main_data));
    return info;
}

static void report_route_tests(void)
{
    struct nrio_boot_rom rom = {.kind = NRIO_BOOT_XULU, .address = 0x40000};
    assert(nrio_report_primary_occupies_secondary(rom, false));
    rom.address = 0x80000;
    assert(!nrio_report_primary_occupies_secondary(rom, false)); /* bypassed, table can survive */
    rom.address = 0x40000;
    rom.kind = NRIO_BOOT_DIRECT_DS;
    assert(nrio_report_primary_occupies_secondary(rom, false));
    rom.kind = NRIO_BOOT_UNKNOWN;
    assert(!nrio_report_primary_occupies_secondary(rom, false));
    assert(nrio_report_primary_occupies_secondary(rom, true)); /* <<GAME>> */
    rom.kind = NRIO_BOOT_STOCK;
    rom.address = 0x20000;
    assert(!nrio_report_primary_occupies_secondary(rom, false));
}

static void route_tests(const u8 *rom)
{
    const uint32_t safe_size = read_word(rom + 0x80);
    assert(safe_size <= 0x20000);
    save("safe-replacement.nds", rom, safe_size);
    u8 *custom = malloc(0x100000);
    assert(custom);
    load("xulu141.nds", custom, 0x100000);
    const uint32_t used = read_word(custom + 0x80);
    save("udisk-replacement.nds", custom, used);

    /* Bypassed cart, with an inactive safe mode and valid secondary map. */
    struct nrio_raw_info info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count==1 && erase_count==1 && erase_pages[0]==0);
    assert(nrio_inspect_boot(0x8000, info.sectors).kind==NRIO_BOOT_PATCHED);
    assert(!memcmp(main_data+0x40000, original_main+0x40000, sizeof(main_data)-0x40000));
    assert(!memcmp(spare_data, original_spare, 8*64));

    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    memset(main_data+0x20000, 255, 0x20000);
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count==1 && erase_count==(pages==64?2:1));
    assert(erase_pages[erase_count-1]==0);
    assert(!memcmp(main_data+0x20000, rom, safe_size));

    /* Bypassed cart with an occupied chunk in the normal safe region. */
    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    memset(main_data+0x20000, 255, 0x20000);
    const unsigned occupied = 0x28000;
    memset(main_data + occupied, 0x57, 0x4000);
    spare_data[occupied / PAGE_SIZE * SPARE_SIZE + 9] = 0x61;
    memcpy(original_main, main_data, sizeof(main_data));
    memcpy(original_spare, spare_data, sizeof(spare_data));
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count == 1 && erase_count == (pages == 64 ? 2 : 1));
    assert(erase_pages[erase_count-1] == 0);
    assert(nrio_inspect_primary_route(info.sectors).kind == NRIO_BOOT_PATCHED);
    assert(read_word(main_data + 0x8008) == 0x2c000);
    for (uint32_t offset = 0; offset < safe_size; offset += 0x4000) {
        const uint32_t address = read_word(main_data + 0x8000 + offset / 0x4000 * 4);
        const uint32_t bytes = safe_size - offset < 0x4000 ? safe_size - offset : 0x4000;
        assert(address != occupied && !memcmp(main_data + address, rom + offset, bytes));
    }
    assert(!memcmp(main_data + occupied, original_main + occupied, 0x4000));
    assert(!memcmp(spare_data + occupied / PAGE_SIZE * SPARE_SIZE,
                   original_spare + occupied / PAGE_SIZE * SPARE_SIZE, 0x4000 / PAGE_SIZE * SPARE_SIZE));
    assert(!memcmp(main_data + 0x40000, original_main + 0x40000, sizeof(main_data) - 0x40000));

    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    memset(main_data+0x20000, 0x57, 0x20000);
    nrio_install_region(".", &info, snapshot, false, false);
    assert(!confirm_count && !erase_count);

    /* Replace secondary payload while the primary still boots safe mode. */
    info = setup(rom);
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(confirm_count==1 && erase_count==(pages==64?9:5));
    assert(erase_pages[erase_count-1]==0x40000/2048);
    assert(!memcmp(main_data,original_main,0x40000));
    assert(!memcmp(main_data+0x80000,custom,used));
    assert(nrio_inspect_boot(0x40000,info.sectors).kind == NRIO_BOOT_XULU);

    /* No secondary table, so the direct route publishes at 0x8000. */
    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    memset(main_data+0x40000,255,0x20000);
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(confirm_count==1 && erase_count==(pages==64?9:5));
    assert(erase_pages[erase_count-1]==0);
    assert(nrio_inspect_boot(0x40000,info.sectors).kind==NRIO_BOOT_ERASED);
    assert(nrio_inspect_boot(0x8000,info.sectors).kind==NRIO_BOOT_XULU);
    assert(!memcmp(spare_data, original_spare, 8*64));

    /* Existing safe mode with an erased secondary table should keep safe mode. */
    info = init_cart(rom);
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(confirm_count==1 && erase_count==(pages==64?9:5));
    assert(erase_pages[erase_count-1]==0x40000/2048);
    assert(nrio_inspect_boot(0x8000,info.sectors).kind == NRIO_BOOT_PATCHED);

    info = init_cart(rom);
    main_data[0x40000]=0;  /* Not genuinely erased and not a verified table. */
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(!confirm_count && !erase_count);
    /* A mapped game at the nominal secondary-table location is not a
       writable table. Either operation keeps those game bytes intact. */
    info = setup_bootleg(rom);
    assert(nrio_inspect_primary_route(info.sectors).kind==NRIO_BOOT_DIRECT_DS);
    assert(nrio_inspect_primary_route(info.sectors).address==0x40000);
    main_data[0x4015e] ^= 1;
    assert(nrio_inspect_primary_route(info.sectors).kind==NRIO_BOOT_UNKNOWN);
    main_data[0x4015e] ^= 1;
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count==1 && erase_count==(pages==64?2:1));
    assert(nrio_inspect_boot(0x8000,info.sectors).kind==NRIO_BOOT_PATCHED);
    assert(!memcmp(main_data+0x40000,original_main+0x40000,0x20000));

    info = setup_bootleg(rom);
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(confirm_count==1 && erase_count==(pages==64?9:5));
    assert(erase_pages[erase_count-1]==0);
    assert(!memcmp(main_data+0x40000,original_main+0x40000,0x20000));

    /* First word FF alone is not sufficient for an absent table. */
    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    memset(main_data+0x40000,255,0x20000);
    main_data[0x40004]=0;
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(!confirm_count && !erase_count);

    info = setup(rom);
    memcpy(main_data+0x8000, main_data+0x40000, 0x4000);
    fail_secondary_read=true;
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(!confirm_count && !erase_count);

    /* A one-allocation replacement is padded to 256 KiB on large NAND. */
    save("udisk-replacement.nds", rom, safe_size);
    info = init_cart(rom);
    nrio_install_custom_udisk(".", &info, snapshot);
    assert(confirm_count==1 && erase_count==2);
    assert(!memcmp(main_data+0x80000,rom,safe_size));
    for (uint32_t i=0x80000+safe_size; i<0xc0000; ++i) assert(main_data[i]==0xff);
    free(custom);
}

static void safe_handoff_warning_test(const u8 *rom)
{
    struct nrio_raw_info info = setup(rom);
    struct nrio_boot_rom live = nrio_inspect_primary_route(info.sectors);
    assert(live.kind == NRIO_BOOT_PATCHED && live.handoff);
    const uint32_t size = read_word(rom + 0x80), arm7 = read_word(rom + 0x30);
    u8 *candidate = malloc(size);
    assert(candidate);
    memcpy(candidate, rom, size);
    unsigned stub = nrio_safe_mode_crc16(candidate + arm7, 0x6c) == 0x1a2d ? 0x6c : 0x3c;
    assert(nrio_safe_mode_handoff(candidate + arm7 + stub) == live.handoff);
    candidate[arm7 + stub + 0x37] ^= 8;
    save("safe-replacement.nds", candidate, size);
    free(candidate);
    permit = false;
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count == 1 && erase_count == 0);
    assert(!memcmp(main_data, original_main, sizeof(main_data)));
}

static void init_marker(const char *main_path, const char *spare_path)
{
    load(main_path, main_data, pages * PAGE_SIZE);
    load(spare_path, spare_data, pages * SPARE_SIZE);
    erase_count=programs=0;
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_SD_BLOCK0_RESTORE));
    assert(nrio_recovery_bind_workspace(workspace,sizeof(workspace)));
}

static void main_marker_tests(const char *main_path, const char *spare_path)
{
    init_marker(main_path,spare_path);
    memcpy(original_main,main_data,pages*PAGE_SIZE);
    memcpy(original_spare,spare_data,pages*SPARE_SIZE);
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_MAIN_READ));
    assert(nrio_recovery_probe_main_marker(snapshot, sizeof(snapshot)) == NRIO_MARKER_ORIGINAL);
    assert(nrio_recovery_backup_pages()==0 && erase_count==0);
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_SD_BLOCK0_RESTORE));
    assert(nrio_recovery_bind_workspace(workspace,sizeof(workspace)));
    assert(nrio_recovery_prepare_main_marker());
    assert(nrio_recovery_backup_pages()==pages && erase_count==0);
    u8 before[PAGE_SIZE], before_spare[SPARE_SIZE];
    assert(nrio_recovery_copy_backup_page(1,before,before_spare));
    assert(!memcmp(before,original_main+PAGE_SIZE,PAGE_SIZE));
    assert(!memcmp(before_spare,original_spare+SPARE_SIZE,SPARE_SIZE));
    assert(nrio_recovery_install_main_marker());
    assert(erase_count==1 && erase_pages[0]==0);
    assert(!memcmp(spare_data,original_spare,pages*SPARE_SIZE));
    static const u8 edited[] = {0xf4, 0x2d, 0x1c, 0xf4, 0xb9, 0x06, 0x53, 0x91};
    assert(!memcmp(main_data+0x808,edited,sizeof(edited)));
    assert(main_data[0x46c]==0x82 && main_data[0x46d]==0x36);
    assert(nrio_safe_mode_crc16(main_data+0x400,0x15e)==
           (main_data[0x55e] | main_data[0x55f]<<8));
    for (unsigned i=0;i<pages*PAGE_SIZE;++i) {
        if (i==0x46c || i==0x46d || i==0x55e || i==0x55f || (i>=0x808 && i<=0x80f)) continue;
        assert(main_data[i]==original_main[i]);
    }
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_MAIN_READ));
    assert(nrio_recovery_probe_main_marker(snapshot, sizeof(snapshot)) == NRIO_MARKER_PATCHED);
    assert(nrio_recovery_probe_main_marker(snapshot, PAGE_RECORD_SIZE) == NRIO_MARKER_READ_ERROR);
    assert(nrio_recovery_startup() && nrio_recovery_select_mode(NRIO_RAW_SD_BLOCK0_RESTORE));
    assert(nrio_recovery_bind_workspace(workspace,sizeof(workspace)));
    assert(nrio_recovery_prepare_main_marker());
    assert(nrio_recovery_backup_pages()==0 && !nrio_recovery_install_main_marker());

    init_marker(main_path,spare_path);
    main_data[0x812]^=1;
    assert(!nrio_recovery_prepare_main_marker() && erase_count==0);

    init_marker(main_path,spare_path);
    spare_data[0x100]^=1;
    assert(!nrio_recovery_prepare_main_marker() && erase_count==0);

    init_marker(main_path,spare_path);
    assert(nrio_recovery_prepare_main_marker());
    main_data[0x2000]^=1;
    assert(!nrio_recovery_install_main_marker() && erase_count==0);
}

static struct nrio_raw_info setup_archived(const u8 *rom, const char *archive)
{
    struct nrio_raw_info info = init_cart(rom);
    FILE *file = fopen(archive, "rb");
    assert(file);
    size_t count = fread(main_data+0x80000, 1, 0x100000, file);
    assert(count >= 0x8200 && count < 0x100000 && fgetc(file) == EOF && !ferror(file));
    assert(!fclose(file));
    const unsigned slots = (count + UDISK_ALLOCATION_BYTES - 1) / UDISK_ALLOCATION_BYTES;
    uint32_t allocations[UDISK_ALLOCATION_COUNT];
    for (unsigned i=0; i<UDISK_ALLOCATION_COUNT; ++i)
        allocations[i] = i < slots ? 0x80000+i*UDISK_ALLOCATION_BYTES : UINT32_MAX;
    udisk_build_secondary_chunk(allocations, main_data+0x40000);
    memcpy(original_main, main_data, sizeof(main_data));
    return info;
}

static void archived_upgrade_tests(const u8 *rom, const char *archive)
{
    struct nrio_raw_info info = setup_archived(rom, archive);
    enum nrio_boot_kind kind = nrio_inspect_boot(0x40000,info.sectors).kind;
    assert(kind==NRIO_BOOT_COMPACT || kind==NRIO_BOOT_UNKNOWN);
    main_data[0x60000-8] = 2;
    nrio_install_region(".", &info, snapshot, true, true);
    assert(confirm_count==1 && erase_count>=2 && erase_count<=(pages==64?7:4));
    assert(erase_pages[erase_count-1]==0x40000/2048);
    assert(!memcmp(main_data, original_main, 0x40000));
    assert(!memcmp(spare_data, original_spare, 0x40000/PAGE_SIZE*SPARE_SIZE));
    assert(!memcmp(main_data+0x140000, original_main+0x140000, sizeof(main_data)-0x140000));
    assert(nrio_inspect_boot(0x40000,info.sectors).kind==NRIO_BOOT_UDISK);
    for (unsigned i=0; i<NRIO_ASSET_UDISK_CHUNKS; ++i) {
        assert(nrio_asset_expand(i, snapshot, UDISK_ALLOCATION_BYTES));
        assert(!memcmp(main_data+0x80000+i*UDISK_ALLOCATION_BYTES, snapshot, UDISK_ALLOCATION_BYTES));
    }

    info = setup_archived(rom, archive);
    main_data[0x80400] ^= 1; /* Beyond the diagnostic signature. */
    nrio_install_region(".", &info, snapshot, true, true);
    assert(confirm_count==0 && erase_count==0);

    info = setup_archived(rom, archive);
    if (read_word(main_data + 0x40000 + 32 * 4) == UINT32_MAX) {
        spare_data[0x100000 / PAGE_SIZE * SPARE_SIZE + 4] = 0xfe;
        nrio_install_region(".", &info, snapshot, true, true);
        assert(confirm_count==0 && erase_count==0);
        info = setup_archived(rom, archive);
        main_data[0x100000] = 0xfe;
        nrio_install_region(".", &info, snapshot, true, true);
        assert(confirm_count==0 && erase_count==0);
    }

    info = setup_archived(rom, archive);
    main_data[0x43000] ^= 1;
    nrio_install_region(".", &info, snapshot, true, true);
    assert(confirm_count==0 && erase_count==0);

    info = setup_archived(rom, archive);
    change_on_confirm = true;
    nrio_install_region(".", &info, snapshot, true, true);
    assert(confirm_count==1 && erase_count==0);

    info = setup_archived(rom, archive);
    corrupt_backup = true;
    nrio_install_region(".", &info, snapshot, true, true);
    assert(confirm_count==1 && erase_count==0);
}

static bool save_bypass(const char *root, const struct nrio_raw_info *info, const u8 *records)
{
    assert(root && info && records);
    assert(!memcmp(records, original_main, PAGE_SIZE));
    return true;
}

static bool read_capture(void *unused, uint32_t lba, uint8_t sector[512])
{
    (void)unused;
    return nrio_recovery_read(lba, 1, sector);
}

static struct nrio_raw_info setup_capture(const u8 *rom, const char *file)
{
    struct nrio_raw_info info = init_cart(rom);
    FILE *fp = fopen(file, "rb");
    assert(fp && !fseek(fp, 0x40000, SEEK_SET));
    assert(fread(main_data + 0x40000, 1, 0x100000, fp) == 0x100000 && !fclose(fp));
    memcpy(original_main, main_data, sizeof(main_data));
    return info;
}

static void fire_capture_tests(const u8 *rom, const char *large, const char *small)
{
    struct nrio_raw_info info = setup_capture(rom, large);
    uint32_t expected_arm9 = nrio_update_crc32(UINT32_MAX, rom + read_word(rom + 0x20),
                                              read_word(rom + 0x2c)) ^ UINT32_MAX;
    struct nrio_boot_rom route = nrio_inspect_primary_route(info.sectors);
    uint32_t actual_arm9;
    assert(nrio_safe_mode_arm9_crc(info.sectors, &route, &actual_arm9));
    assert(actual_arm9 == expected_arm9);
    assert(nrio_identify_udisk_capture(read_capture, NULL, info.sectors) == NRIO_UDISK_CAPTURE_FIRE16);
    nrio_bypass_safe_mode(".", &info, snapshot, save_bypass);
    assert(confirm_count == 1 && erase_count == 1 && erase_pages[0] == 0);
    assert(!memcmp(main_data, original_main, 0x8000));
    assert(!memcmp(main_data + 0x8000, original_main + 0x40000, 0x4000));
    assert(!memcmp(main_data + 0xc000, original_main + 0xc000, sizeof(main_data) - 0xc000));
    const unsigned first = 0x8000 / PAGE_SIZE * SPARE_SIZE;
    const unsigned last = 0xc000 / PAGE_SIZE * SPARE_SIZE;
    assert(!memcmp(spare_data, original_spare, first));
    assert(!memcmp(spare_data + last, original_spare + last, sizeof(spare_data) - last));

    info = setup_capture(rom, large);
    main_data[0x43000] ^= 1;
    nrio_bypass_safe_mode(".", &info, snapshot, save_bypass);
    assert(confirm_count == 0 && erase_count == 0);

    info = setup_capture(rom, large);
    main_data[0x13fffd] ^= 1;
    nrio_bypass_safe_mode(".", &info, snapshot, save_bypass);
    assert(confirm_count == 0 && erase_count == 0);

    info = setup_capture(rom, large);
    change_on_confirm = true;
    nrio_bypass_safe_mode(".", &info, snapshot, save_bypass);
    assert(confirm_count == 1 && erase_count == 0);

    info = setup_capture(rom, large);
    nrio_install_hidden_xulu(".", &info, snapshot);
    assert(confirm_count == 0 && erase_count == 0); /* exception is bypass only */

    info = setup_capture(rom, small);
    assert(nrio_identify_udisk_capture(read_capture, NULL, info.sectors) == NRIO_UDISK_CAPTURE_FIRE8);
    nrio_bypass_safe_mode(".", &info, snapshot, save_bypass);
    assert(confirm_count == 0 && erase_count == 0);
}

/* The supplied backup ends at 0x40000. Model the uncaptured secondary
   map and payload using the active primary map and archived XuluMenu. */
static void fpga_capture_test(const u8 *rom, const char *main_path, const char *spare_path, const char *manifest)
{
    struct nrio_raw_info info = init_cart(rom);
    u8 anchors[1048];
    load(main_path, main_data, 0x40000);
    load(spare_path, spare_data, 0x2000);
    load(manifest, anchors, sizeof(anchors));
    assert(!memcmp(anchors, "NRIOB01", 8));
    assert(read_word(anchors + 8) == 0x2514d3ec && read_word(anchors + 12) == 128);
    assert(nrio_update_crc32(UINT32_MAX, main_data, 0x40000) == read_word(anchors + 16));
    assert(nrio_update_crc32(UINT32_MAX, spare_data, 0x2000) == read_word(anchors + 20));
    assert(!memcmp(main_data + 0x8000, anchors + 24, 512));
    memcpy(main_data + 0x40000, main_data + 0x8000, 0x4000);
    load("xulu141.nds", main_data + 0x80000, 0x100000);
    assert(!memcmp(main_data + 0x80000, anchors + 536, 512));
    assert(nrio_inspect_primary_route(info.sectors).kind == NRIO_BOOT_XULU);
    const uint32_t size = read_word(main_data + 0x20080);
    assert(size == 0x13e40);
    save("safe-replacement.nds", main_data + 0x20000, size);
    memcpy(original_main, main_data, sizeof(main_data));
    memcpy(original_spare, spare_data, sizeof(spare_data));
    nrio_install_region(".", &info, snapshot, false, false);
    assert(confirm_count == 1 && erase_count == 1 && erase_pages[0] == 0);
    const struct nrio_boot_rom after = nrio_inspect_primary_route(info.sectors);
    assert(after.kind == NRIO_BOOT_PATCHED && after.handoff == 0x1118027e && after.address == 0x20000);
    assert(!memcmp(main_data, original_main, 0x8000));
    assert(!memcmp(main_data + 0xc000, original_main + 0xc000, sizeof(main_data) - 0xc000));
    const unsigned lo = 0x8000 / PAGE_SIZE * SPARE_SIZE, hi = 0xc000 / PAGE_SIZE * SPARE_SIZE;
    assert(!memcmp(spare_data, original_spare, lo));
    assert(!memcmp(spare_data + hi, original_spare + hi, sizeof(spare_data) - hi));
    puts("FPGA capture: reactivated original patched safe mode. Only primary map and its ECC changed.");
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    report_route_tests();
    u8 *rom=malloc(0x1c000);
    assert(rom);
    load("patched.nds",rom,0x1c000);
    for (pages=64;pages<=128;pages*=2) {

        struct nrio_raw_info info=setup(rom);
        nrio_install_hidden_xulu(".", &info, snapshot);
        assert(confirm_count==1 && erase_count==(pages==64?9:5));
        assert(erase_pages[erase_count-1]==0x40000/2048);
        assert(!memcmp(main_data,original_main,0x40000));
        assert(!memcmp(main_data+0x180000,original_main+0x180000,sizeof(main_data)-0x180000));
        assert(!memcmp(spare_data,original_spare,0x40000/PAGE_SIZE*SPARE_SIZE));
        assert(nrio_inspect_boot(0x40000,info.sectors).kind==NRIO_BOOT_XULU);
        u8 *expected=malloc(0x100000);
        assert(expected);
        load("xulu141.nds",expected,0x100000);
        assert(!memcmp(main_data+0x80000,expected,0x100000));
        free(expected);
        confirm_count=erase_count=0;
        nrio_install_hidden_xulu(".", &info, snapshot);
        assert(confirm_count==0 && erase_count==0);

        info=setup(rom);
        spare_data[0x140000/PAGE_SIZE*SPARE_SIZE+1]=0;
        nrio_install_hidden_xulu(".", &info, snapshot);
        assert(confirm_count==0 && erase_count==0);

        info=setup(rom);
        permit=false;
        nrio_install_hidden_xulu(".", &info, snapshot);
        assert(confirm_count==1 && erase_count==0);

        info=setup(rom);
        change_on_confirm=true;
        nrio_install_hidden_xulu(".", &info, snapshot);
        assert(confirm_count==1 && erase_count==0);
        route_tests(rom);
        safe_handoff_warning_test(rom);
        if (argc == 3 && !strcmp(argv[1], "--archive")) archived_upgrade_tests(rom, argv[2]);
        else if (argc == 4 && !strcmp(argv[1], "--fire")) fire_capture_tests(rom, argv[2], argv[3]);
        else if (argc == 5 && !strcmp(argv[1], "--fpga") && pages == 128)
            fpga_capture_test(rom, argv[2], argv[3], argv[4]);
        else if (argc == 3) main_marker_tests(argv[1],argv[2]);
    }
    free(rom);
    if (argc == 3 && !strcmp(argv[1], "--archive"))
        puts("Archived firmware upgraded and refused corrupt or changed sources.");
    else if (argc == 3) puts("Main ARM9 marker patch preserved all spare data and rejected mismatches.");
    if (argc == 4) puts("Fire Link map recognition and Fire16 bypass passed for both geometries.");
    puts("XuluMenu, safe-mode provisioning and custom uDisk routing passed for both erase geometries.");
}
