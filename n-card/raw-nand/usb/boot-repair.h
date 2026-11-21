#ifndef NRIO_BOOT_REPAIR_H
#define NRIO_BOOT_REPAIR_H

#include <stdbool.h>

#include "recovery_nand/recovery.h"

extern uint32_t raw_workspace[NRIO_RAW_WORKSPACE_BYTES / sizeof(uint32_t)];
void boot_repair(bool check_only);

#endif
