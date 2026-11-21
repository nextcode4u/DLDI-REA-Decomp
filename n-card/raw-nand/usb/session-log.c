#include "session-log.h"
#include "nrio_raw_dldi.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    FAT_SECTOR = 1,
    ROOT_SECTOR = 3,
    DATA_SECTOR = 4,
    LOG_SPACE = (SESSION_LOG_SECTORS - DATA_SECTOR) * SESSION_LOG_SECTOR_BYTES,
};

// Store text in the data area from the start. USB export freezes the directory and FAT.
static uint8_t image[SESSION_LOG_SECTORS * SESSION_LOG_SECTOR_BYTES];
static size_t text_size;
static unsigned event_number;
static bool frozen, full;

void session_log(const char *format, ...)
{
    char line[160];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    puts(line);
    if (frozen || full) return;

    static const char truncated[] = "Log capacity reached. Later events are on-screen only.\r\n";
    char *data = (char *)image + DATA_SECTOR * SESSION_LOG_SECTOR_BYTES;
    size_t available = LOG_SPACE - text_size - sizeof(truncated);
    int written = snprintf(data + text_size, available, "%04u %s\r\n", ++event_number, line);
    if (written >= 0 && (size_t)written < available) {
        text_size += (size_t)written;
    } else {
        memcpy(data + text_size, truncated, sizeof(truncated) - 1);
        text_size += sizeof(truncated) - 1;
        full = true;
    }
}

void session_log_write_error(const struct nrio_raw_write_error *error)
{
    static const char *const stages[] = {
        [NRIO_RAW_WRITE_READ_BLOCK] = "read before erase",
        [NRIO_RAW_WRITE_MARKER] = "spare preflight",
        [NRIO_RAW_WRITE_ERASE] = "erase",
        [NRIO_RAW_WRITE_PROGRAM] = "program",
        [NRIO_RAW_WRITE_VERIFY_READ] = "readback",
        [NRIO_RAW_WRITE_VERIFY_DATA] = "readback mismatch",
        [NRIO_RAW_WRITE_TABLE_SOURCE] = "source table changed or unreadable",
    };
    if (error->stage && error->stage < sizeof(stages) / sizeof(stages[0])) {
        session_log("%s\nPage 0x%lx, detail 0x%lx.", stages[error->stage],
                    (unsigned long)error->page, (unsigned long)error->detail);
    }
}

static void put16(uint8_t *p, unsigned value)
{
    p[0] = value;
    p[1] = value >> 8;
}

static void put32(uint8_t *p, uint32_t value)
{
    put16(p, value);
    put16(p + 2, value >> 16);
}

static void set_cluster(uint8_t *fat, unsigned cluster, unsigned next)
{
    uint8_t *entry = fat + cluster + cluster / 2;
    if (cluster & 1) {
        entry[0] = (entry[0] & 0x0f) | (next << 4);
        entry[1] = next >> 4;
    } else {
        entry[0] = next;
        entry[1] = (entry[1] & 0xf0) | (next >> 8);
    }
}

void session_log_prepare_disk(void)
{
    frozen = true;
    memset(image, 0, DATA_SECTOR * SESSION_LOG_SECTOR_BYTES);
    memset(image + DATA_SECTOR * SESSION_LOG_SECTOR_BYTES + text_size, 0, LOG_SPACE - text_size);
    static const uint8_t boot[] = {0xeb, 0x3c, 0x90};
    memcpy(image, boot, sizeof(boot));
    memcpy(image + 3, "MSDOS5.0", 8);
    put16(image + 11, SESSION_LOG_SECTOR_BYTES);
    image[13] = 1;
    put16(image + 14, 1);
    image[16] = 2;
    put16(image + 17, 16);
    put16(image + 19, SESSION_LOG_SECTORS);
    image[21] = 0xf8;
    put16(image + 22, 1);
    put16(image + 24, 32);
    put16(image + 26, 1);
    image[36] = 0x80;
    image[38] = 0x29;
    put32(image + 39, 0x4e52494f);
    memcpy(image + 43, "NRIO LOGS  FAT12   ", 19);
    put16(image + 510, 0xaa55);

    uint8_t *fat = image + FAT_SECTOR * SESSION_LOG_SECTOR_BYTES;
    set_cluster(fat, 0, 0xff8);
    set_cluster(fat, 1, 0xfff);
    unsigned clusters = (text_size + SESSION_LOG_SECTOR_BYTES - 1) / SESSION_LOG_SECTOR_BYTES;
    for (unsigned i = 0; i < clusters; ++i)
        set_cluster(fat, i + 2, i + 1 == clusters ? 0xfff : i + 3);
    memcpy(fat + SESSION_LOG_SECTOR_BYTES, fat, SESSION_LOG_SECTOR_BYTES);

    uint8_t *root = image + ROOT_SECTOR * SESSION_LOG_SECTOR_BYTES;
    memcpy(root, "NRIO LOGS  ", 11);
    root[11] = 0x08;
    uint8_t *file = root + 32;
    memcpy(file, "SESSION TXT", 11);
    file[11] = 0x21;
    put16(file + 16, 0x0021);
    put16(file + 18, 0x0021);
    put16(file + 24, 0x0021);
    put16(file + 26, clusters ? 2 : 0);
    put32(file + 28, text_size);
}

bool session_log_read_disk(uint32_t lba, uint32_t offset, void *buffer, uint32_t size)
{
    if (!frozen || lba >= SESSION_LOG_SECTORS || offset >= SESSION_LOG_SECTOR_BYTES) return false;
    uint32_t start = lba * SESSION_LOG_SECTOR_BYTES + offset;
    if (size > sizeof(image) - start) return false;
    memcpy(buffer, image + start, size);
    return true;
}
