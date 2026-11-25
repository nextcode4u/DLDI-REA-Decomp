#include <assert.h>
#include <string.h>
#include "../sector-zero.h"

struct media {
    uint8_t live[NRIO_SECTOR0_BYTES], saved[NRIO_SECTOR0_BYTES];
    unsigned reads, writes, backups, confirms, restarts;
    bool read_error, backup_error, cancel, swapped, write_error, mismatch, restart_error;
};

static bool read(void *ctx, uint8_t *out)
{
    struct media *m = ctx;
    ++m->reads;
    if (m->read_error) return false;
    memcpy(out, m->live, sizeof(m->live));
    if (m->writes && m->mismatch) out[8] ^= 1;
    return true;
}
static bool write(void *ctx, const uint8_t *in)
{
    struct media *m = ctx;
    assert(m->restarts == 1 && m->backups == 1 && m->confirms == 1);
    ++m->writes;
    if (m->write_error) return false;
    memcpy(m->live, in, sizeof(m->live));
    return true;
}
static bool restart(void *ctx)
{
    struct media *m = ctx;
    ++m->restarts;
    return !(m->restart_error && m->writes);
}
static bool backup(void *ctx, const uint8_t *data)
{
    struct media *m = ctx;
    ++m->backups;
    memcpy(m->saved, data, sizeof(m->saved));
    return !m->backup_error;
}
static bool confirm(void *ctx)
{
    struct media *m = ctx;
    ++m->confirms;
    if (m->swapped) m->live[10] ^= 1;
    return !m->cancel;
}

int main(void)
{
    uint8_t input[NRIO_SECTOR0_BYTES] = {
        [0] = 0xeb, [12] = 2, [13] = 128, [14] = 8, [16] = 2, [510] = 0x55, [511] = 0xaa,
    };
    uint8_t invalid[NRIO_SECTOR0_BYTES] = {0}, loaded[NRIO_SECTOR0_BYTES];
    FILE *file = tmpfile();
    assert(file);
    assert(fwrite(input, 1, sizeof(input), file) == sizeof(input));
    rewind(file);
    assert(nrio_sector0_load(file, loaded) && !memcmp(input, loaded, sizeof(input)));
    assert(!fseek(file, 0, SEEK_END) && fputc(0, file) == 0);
    rewind(file);
    assert(!nrio_sector0_load(file, loaded));
    assert(!fclose(file));
    file = tmpfile();
    assert(file && fputc(0, file) == 0);
    rewind(file);
    assert(!nrio_sector0_load(file, loaded));
    assert(!fclose(file));

    struct media m = {0};
    const struct nrio_sector0_io io = {&m, read, write, restart, backup, confirm};
    assert(nrio_sector0_restore(&io, invalid) == NRIO_SECTOR0_BAD_IMAGE && !m.reads);
    m = (struct media){.read_error = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_READ_FAILED && !m.writes);
    m = (struct media){.backup_error = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_BACKUP_FAILED && !m.confirms);
    m = (struct media){.cancel = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_CANCELLED && !m.writes);
    m = (struct media){.swapped = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_CHANGED && !m.writes);
    m = (struct media){.write_error = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_WRITE_FAILED && m.restarts == 1);
    m = (struct media){.mismatch = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_VERIFY_FAILED && m.restarts == 2);
    m = (struct media){.restart_error = true};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_VERIFY_FAILED && m.restarts == 2);
    m = (struct media){0};
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_OK && m.writes == 1 && m.reads == 3);
    assert(!memcmp(m.live, input, sizeof(m.live)) && !memcmp(m.saved, invalid, sizeof(m.saved)));
    assert(nrio_sector0_restore(&io, input) == NRIO_SECTOR0_SAME && m.writes == 1);
    puts("FAT sector-0 checks passed for exact size, backup, write and remount verification.");
}
