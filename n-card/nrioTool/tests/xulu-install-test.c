/* Host tests for the migration from the old filesystem-only loader. */
#include "../xulu-install.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

static const char *const payload[] = {"Apache XuluMenu test", "Stock XMenu test"};
static bool fail_stream;

uint32_t nrio_update_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
    return (uint32_t)crc32(crc ^ UINT32_MAX, data, len) ^ UINT32_MAX;
}

static struct nrio_file_id id(const char *s)
{
    return (struct nrio_file_id){strlen(s), crc32(0, (const unsigned char *)s, strlen(s))};
}

static bool stream(void *context, FILE *f)
{
    const char *s = payload[*(unsigned *)context];
    return !fail_stream && fwrite(s, 1, strlen(s), f) == strlen(s);
}

static void save(const char *path, const char *s)
{
    FILE *f = fopen(path, "wb");
    assert(f && fputs(s, f) != EOF && !fclose(f));
}

static enum nrio_xulu_result install(void)
{
    return nrio_restore_stock_menu(".", id(payload[0]), id(payload[1]), stream, 1);
}

int main(void)
{
    char temp[] = "/tmp/nrio-xulu-test.XXXXXX";
    assert(mkdtemp(temp) && !chdir(temp));
    assert(install() == NRIO_XULU_OK);
    assert(nrio_file_matches("xmenu.dat", id(payload[1])));
    assert(install() == NRIO_XULU_ALREADY);
    assert(access("xulu-old.nds", F_OK));

    save("xmenu.dat", "some other loader");
    assert(install() == NRIO_XULU_UNCHANGED);
    assert(nrio_file_matches("xmenu.dat", id("some other loader")));

    save("xmenu.dat", payload[0]);
    fail_stream = true;
    assert(install() == NRIO_XULU_ERROR);
    assert(nrio_file_matches("xmenu.dat", id(payload[0])));
    fail_stream = false;
    save("xulu-old.nds", "unrelated backup");
    assert(install() == NRIO_XULU_ALIAS_CONFLICT);
    assert(nrio_file_matches("xulu-old.nds", id("unrelated backup")));
    assert(!unlink("xulu-old.nds"));

    assert(install() == NRIO_XULU_OK);
    assert(nrio_file_matches("xmenu.dat", id(payload[1])));
    assert(nrio_file_matches("xulu-old.nds", id(payload[0])));
    assert(!unlink("xulu-old.nds") && !unlink("xmenu.dat"));
    assert(!chdir("/") && !rmdir(temp));
    puts("Stock XMenu setup and legacy-wrapper migration passed.");
    return 0;
}
