#include "../file-transfer.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void save(const char *name, unsigned count, unsigned salt)
{
    FILE *f = fopen(name, "wb");
    assert(f);
    for (unsigned i = 0; i < count; ++i) assert(fputc((i + salt) % 251, f) != EOF);
    assert(!fclose(f));
}

static void check(const char *name, unsigned count, unsigned salt)
{
    FILE *f = fopen(name, "rb");
    assert(f);
    for (unsigned i = 0; i < count; ++i) assert(fgetc(f) == (int)((i + salt) % 251));
    assert(fgetc(f) == EOF && !ferror(f) && !fclose(f));
}

static bool cancel_verify(void *p, bool verify, uint64_t done, uint64_t total)
{
    (void)p;
    assert(done <= total);
    return !verify;
}

int main(void)
{
    char cwd[] = "/tmp/nrio-transfer.XXXXXX";
    assert(mkdtemp(cwd) && !chdir(cwd));
    assert(!mkdir("sd:", 0700) && !mkdir("cart:", 0700));
    const char *src = "sd:/new", *dst = "cart:/new";
    save(src, 90000, 17);
    save(dst, 123, 42);

    assert(nrio_file_transfer(src, dst, false, false, NULL, NULL) == NRIO_FILE_EXISTS);
    assert(nrio_file_transfer(src, dst, true, true, cancel_verify, NULL) == NRIO_FILE_CANCELLED);
    check(src, 90000, 17);
    check(dst, 123, 42);

    assert(nrio_file_transfer(src, dst, false, true, NULL, NULL) == NRIO_FILE_OK);
    check(src, 90000, 17);
    check(dst, 90000, 17);
    assert(!unlink(dst));
    assert(nrio_file_transfer(src, dst, true, false, NULL, NULL) == NRIO_FILE_OK);
    assert(access(src, F_OK));
    check(dst, 90000, 17);

    assert(nrio_file_transfer(dst, "cart:/other", true, false, NULL, NULL) == NRIO_FILE_OK);
    assert(access(dst, F_OK));
    assert(nrio_file_transfer("cart:/other", "CART:/OTHER", true, true, NULL, NULL) == NRIO_FILE_FAILED);
    check("cart:/other", 90000, 17);
    assert(nrio_file_transfer("cart:/other", "sd:", true, true, NULL, NULL) == NRIO_FILE_FAILED);
    assert(nrio_file_valid_name("hello world.nds") && !nrio_file_valid_name("../a") &&
           !nrio_file_valid_name("test.") && !nrio_file_valid_name("sd:"));
    save(src, 0, 0);
    assert(nrio_file_transfer(src, dst, true, false, NULL, NULL) == NRIO_FILE_OK);
    check(dst, 0, 0);
    assert(!unlink(dst) && !unlink("cart:/other") && !rmdir("cart:") && !rmdir("sd:"));
    assert(!chdir("/") && !rmdir(cwd));
    puts("File transfer checks passed.");
}
