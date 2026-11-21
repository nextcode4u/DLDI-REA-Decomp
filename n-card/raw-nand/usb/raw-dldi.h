#ifndef NRIO_RAW_DLDI_PROTOCOL_H
#define NRIO_RAW_DLDI_PROTOCOL_H

#include <stdint.h>

#define NRIO_RAW_INFO_LBA UINT32_C(0xffffffff)
#define NRIO_RAW_WORKSPACE_LBA UINT32_C(0xfffffffe)
#define NRIO_RAW_WRITE_ERROR_LBA UINT32_C(0xfffffffd)
#define NRIO_RAW_MODE_LBA UINT32_C(0xfffffffc)
#define NRIO_RAW_REPAIR_TABLE_LBA UINT32_C(0xfffffffb)
#define NRIO_RAW_ABI_VERSION UINT32_C(1)
#define NRIO_RAW_WORKSPACE_BYTES (128u * (2048u + 64u))
#define NRIO_RAW_PRIMARY_TABLE UINT32_C(0x8000)
#define NRIO_RAW_SECONDARY_TABLE UINT32_C(0x40000)
#define NRIO_RAW_TABLE_BYTES UINT32_C(0x4000)
#define NRIO_RAW_SHARED_SPARE_BYTES UINT32_C(512)

/* Private DLDI calls use count=0 for info and count=workspace bytes for binding. */
struct nrio_raw_info {
    uint32_t version;
    uint32_t sectors;
    uint32_t workspace_bytes;
    uint32_t nand_id;
};

enum nrio_raw_write_stage {
    NRIO_RAW_WRITE_NONE,
    NRIO_RAW_WRITE_READ_BLOCK,
    NRIO_RAW_WRITE_MARKER,
    NRIO_RAW_WRITE_ERASE,
    NRIO_RAW_WRITE_PROGRAM,
    NRIO_RAW_WRITE_VERIFY_READ,
    NRIO_RAW_WRITE_VERIFY_DATA,
    NRIO_RAW_WRITE_TABLE_SOURCE,
};

/* Toolkit mode selection is a one-time private read with count=0. */
enum nrio_raw_mode {
    NRIO_RAW_MAIN_READ,
    NRIO_RAW_SPARE_READ,
    NRIO_RAW_MAIN_PROTECTED,
    NRIO_RAW_MAIN_WRITE,
    NRIO_RAW_SPARE_WRITE,
    NRIO_RAW_BANNER_WRITE,
    NRIO_RAW_TABLE_REPAIR,
};

struct nrio_raw_table_plan {
    uint8_t table[NRIO_RAW_TABLE_BYTES];
    uint8_t expected_spare[NRIO_RAW_SHARED_SPARE_BYTES];
};

struct nrio_raw_write_error {
    uint32_t stage;
    uint32_t page;
    uint32_t detail;
};

#endif
