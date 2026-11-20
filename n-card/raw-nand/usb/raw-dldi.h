#ifndef NRIO_RAW_DLDI_PROTOCOL_H
#define NRIO_RAW_DLDI_PROTOCOL_H

#include <stdint.h>

#define NRIO_RAW_INFO_LBA UINT32_C(0xffffffff)
#define NRIO_RAW_WORKSPACE_LBA UINT32_C(0xfffffffe)
#define NRIO_RAW_ABI_VERSION UINT32_C(1)
#define NRIO_RAW_WORKSPACE_BYTES (128u * (2048u + 64u))

/* Private DLDI calls use count=0 for info and count=workspace bytes for binding. */
struct nrio_raw_info {
    uint32_t version;
    uint32_t sectors;
    uint32_t workspace_bytes;
    uint32_t nand_id;
};

#endif
