/* Host tests for the filesystem-only loader switch. */
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

static enum nrio_xulu_result install(bool allow)
{
    return nrio_install_xulu_files(".", allow, id(payload[0]), id(payload[1]), stream, 0, 1);
}

int main(void)
{
    char temp[] = "/tmp/nrio-xulu-test.XXXXXX";
    assert(mkdtemp(temp) && !chdir(temp));
    assert(install(false) == NRIO_XULU_OK);
    assert(nrio_file_matches("xmenu.dat", id(payload[0])));
    assert(nrio_file_matches("xmenu.nds", id(payload[1])));
    assert(install(false) == NRIO_XULU_ALREADY);
    assert(!unlink("xmenu.dat") && !unlink("xmenu.nds"));

    save("xmenu.dat", payload[1]);
    assert(install(false) == NRIO_XULU_UNCHANGED);
    assert(nrio_file_matches("xmenu.dat", id(payload[1])) && access("xmenu.nds", F_OK));
    fail_stream = true;
    assert(install(true) == NRIO_XULU_ERROR);
    assert(nrio_file_matches("xmenu.dat", id(payload[1])) && access("xmenu.nds", F_OK));
    fail_stream = false;
    assert(install(true) == NRIO_XULU_OK);
    assert(nrio_file_matches("xmenu.nds", id(payload[1])));
    assert(nrio_file_matches("xmenu.dat", id(payload[0])));

    save("xmenu.dat", "some other loader");
    assert(install(false) == NRIO_XULU_UNCHANGED);
    assert(install(true) == NRIO_XULU_OK);
    assert(nrio_file_matches("xmenu-old.nds", id("some other loader")));
    assert(nrio_file_matches("xmenu.nds", id(payload[1])));
    assert(nrio_file_matches("xmenu.dat", id(payload[0])));

    save("xmenu.dat", "a different loader");
    assert(install(true) == NRIO_XULU_ALIAS_CONFLICT);
    assert(nrio_file_matches("xmenu.dat", id("a different loader")));

    assert(!unlink("xmenu-old.nds") && !unlink("xmenu.dat") && !unlink("xmenu.nds"));
    assert(!chdir("/") && !rmdir(temp));
    puts("XuluMenu fresh install, explicit switch, backup preservation and refusal checks passed.");
    return 0;
}
