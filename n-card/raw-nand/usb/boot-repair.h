#ifndef NRIO_BOOT_REPAIR_H
#define NRIO_BOOT_REPAIR_H

#include <stdbool.h>

#include "recovery_nand/recovery.h"

extern uint32_t raw_workspace[NRIO_RAW_WORKSPACE_BYTES / sizeof(uint32_t)];
void boot_repair(bool check_only);
void install_safe_mode(void);

enum safe_mode_status {
    SAFE_MODE_UNKNOWN,
    SAFE_MODE_UNREADABLE,
    SAFE_MODE_STOCK,
    SAFE_MODE_TOOLKIT,
    SAFE_MODE_APACHE,
};

struct safe_mode_info {
    uint32_t rom_address;
    uint32_t arm7_address;
    uint32_t handoff;
};

enum safe_mode_status read_safe_mode_status(struct safe_mode_info *info);
void check_safe_mode(void);

#endif
