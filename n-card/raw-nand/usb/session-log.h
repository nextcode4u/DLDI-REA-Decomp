#ifndef NRIO_SESSION_LOG_H
#define NRIO_SESSION_LOG_H

#include <stdbool.h>
#include <stdint.h>

enum { SESSION_LOG_SECTORS = 256, SESSION_LOG_SECTOR_BYTES = 512 };

struct nrio_raw_write_error;

void session_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void session_log_write_error(const struct nrio_raw_write_error *error);
void session_log_prepare_disk(void);
bool session_log_read_disk(uint32_t lba, uint32_t offset, void *buffer, uint32_t size);

#endif
