#ifndef NRIO_H
#define NRIO_H

#include <nds.h>

/* The linker sorts these sections in original binary order. */
#define NRIO_ROUTINE(offset) __attribute__((section(".nrio." #offset)))

#endif
