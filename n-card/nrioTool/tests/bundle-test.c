/* Host regression for the RAM-resident firmware and starter pack. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "../bundle-assets.h"
#include "../udisk-setup-io.h"

uint32_t nrio_update_crc32(uint32_t crc, const uint8_t *data, size_t size)
{
    return (uint32_t)crc32(crc ^ UINT32_MAX, data, size) ^ UINT32_MAX;
}

static bool copy_asset(void *ctx, FILE *out)
{
    return nrio_asset_stream(*(const unsigned *)ctx, out);
}

int main(void)
{
    uint8_t *chunk = malloc(0x20000);
    assert(chunk);
    uint32_t crc = UINT32_MAX;
    for (unsigned i = 0; i < NRIO_ASSET_UDISK_CHUNKS; ++i) {
        assert(!nrio_asset_expand(i, chunk, 0x1ffff));
        assert(nrio_asset_expand(i, chunk, 0x20000));
        crc = nrio_update_crc32(crc, chunk, 0x20000);
    }
    assert((crc ^ UINT32_MAX) == 0x3dbef02e);
    size_t driver_length;
    uint32_t driver_crc;
    assert(nrio_asset_info(NRIO_ASSET_DLDI, &driver_length, &driver_crc));
    assert(driver_length >= 0x80 && driver_length <= 0x8000);
    assert(nrio_asset_expand(NRIO_ASSET_DLDI, chunk, driver_length));
    assert(!memcmp(chunk + 0x60, "NRIO", 4) && chunk[0x64] == 0x23);
    free(chunk);

    char root[] = "/tmp/nrio-starter-test.XXXXXX";
    assert(mkdtemp(root));
    for (unsigned i = NRIO_ASSET_XMENU; i < NRIO_ASSET_COUNT; ++i) {
        char file[112], tmp[112];
        snprintf(file, sizeof(file), "%s/file%u", root, i);
        snprintf(tmp, sizeof(tmp), "%s/tmp%u", root, i);
        struct nrio_file_id id;
        assert(nrio_asset_info(i, &id.bytes, &id.crc));
        assert(nrio_write_new_file(file, tmp, id, copy_asset, &i) == NRIO_FILE_OK);
        assert(nrio_file_matches(file, id));
        if (i == NRIO_ASSET_XULU) {
            FILE *fp = fopen(file, "rb");
            unsigned char head[0x40];
            assert(fp && fread(head, 1, sizeof(head), fp) == sizeof(head) && !fclose(fp));
            assert(id.bytes >= 0x8000 && id.bytes <= 0x80000);
            assert(head[0x2c] || head[0x2d] || head[0x2e] || head[0x2f]);
            assert(head[0x3c] || head[0x3d] || head[0x3e] || head[0x3f]);
        }
        assert(nrio_write_new_file(file, tmp, id, copy_asset, &i) == NRIO_FILE_EXISTS);
        assert(access(tmp, F_OK) && !remove(file));
    }
    assert(!rmdir(root));
    puts("Bundled firmware, target DLDI, XuluMenu and stock starter files verified.");
    return 0;
}
