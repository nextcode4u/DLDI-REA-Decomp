#ifndef NRIO_BOOT_REPAIR_H
#define NRIO_BOOT_REPAIR_H

#include "nrio_raw_dldi.h"

extern uint32_t raw_workspace[NRIO_RAW_WORKSPACE_BYTES / sizeof(uint32_t)];
void boot_repair(void);

#endif
