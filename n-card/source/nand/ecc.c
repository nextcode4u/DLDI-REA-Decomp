#include "../nrio.h"

extern u8 nrio_count_bits(u32 value);

NRIO_ROUTINE(13f8) int nrio_correct_ecc(const u8 *expected, const u8 *actual, u8 *data)
{
    u8 low = expected[0] ^ actual[0];
    u8 high = expected[1] ^ actual[1];
    u8 bit = expected[2] ^ actual[2];

    if ((low | high | bit) == 0) {
        return 0;
    }

    if (((low ^ (low >> 1)) & 0x55) == 0x55 &&
        ((high ^ (high >> 1)) & 0x55) == 0x55 &&
        ((bit ^ (bit >> 1)) & 0x55) == 0x55) {
        u32 byte_index = high;
        /* Keep narrowing separate so GCC 4.1.1 reuses the high-byte register. */
        __asm__ ("" : "+r"(byte_index));
        byte_index &= 0x80;
        u32 bit_index = 0;
        if (high & 0x20) {
            byte_index = byte_index == 0 ? 0x40 : 0xc0;
        }
        if (high & 0x08) {
            byte_index |= 0x20;
        }
        if (high & 0x02) {
            byte_index |= 0x10;
        }
        if (low & 0x80) {
            byte_index |= 0x08;
        }
        if (low & 0x20) {
            byte_index |= 0x04;
        }
        if (low & 0x08) {
            byte_index |= 0x02;
        }
        if (low & 0x02) {
            byte_index |= 0x01;
        }
        if (bit & 0x80) {
            bit_index |= 4;
        }
        if (bit & 0x20) {
            bit_index = bit_index == 0 ? 2 : 6;
        }
        if (bit & 0x08) {
            bit_index |= 1;
        }
        if (bit & 0x02) {
            byte_index |= 0x100;
        }
        data[byte_index] ^= 1 << bit_index;
        return 1;
    }

    u32 error_bits = nrio_count_bits(low) + nrio_count_bits(high) + nrio_count_bits(bit);
    return error_bits == 1 ? 2 : 3;
}
