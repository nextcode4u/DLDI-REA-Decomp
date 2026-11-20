#include "../nrio.h"

NRIO_ROUTINE(13c4) void nrio_require_word_alignment(u32 address, const char *message)
{
    (void)message;
    if ((address & 3) != 0) {
        for (;;) {
        }
    }
}

NRIO_ROUTINE(13d0) u8 nrio_count_bits(u32 value)
{
    u8 count = 0;

    while (value != 0) {
        if (value & 1) {
            ++count;
        }
        value >>= 1;
    }
    return count;
}
