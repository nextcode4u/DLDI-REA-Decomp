#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "../block0-backup-check.h"

static uint32_t hash(uint32_t state, const uint8_t *data, size_t size)
{
    while (size--) state = (state ^ *data++) * 16777619u;
    return state;
}

int main(void)
{
    char path[] = "/tmp/nrio-backup-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    uint8_t bytes[650];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    uint32_t expected = hash(UINT32_MAX, bytes, sizeof(bytes));
    assert(write(fd, bytes, sizeof(bytes)) == sizeof(bytes));
    assert(nrio_backup_check_file(path, sizeof(bytes), expected, hash));
    assert(!nrio_backup_check_file(path, sizeof(bytes)-1, hash(UINT32_MAX, bytes, sizeof(bytes)-1), hash));
    assert(!nrio_backup_check_file(path, sizeof(bytes)+1, expected, hash));
    uint8_t bad = 0;
    assert(pwrite(fd, &bad, 1, 517) == 1);
    assert(!nrio_backup_check_file(path, sizeof(bytes), expected, hash));
    assert(close(fd) == 0 && unlink(path) == 0);
    assert(!nrio_backup_check_file(path, sizeof(bytes), expected, hash));
}
