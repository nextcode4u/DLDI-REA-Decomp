#include "../ftl/state.h"

extern void nrio_send_nand_command(u32 command);
extern void nrio_command_55(u32 pages);
extern void nrio_command_52(u32 column);
extern void nrio_wait_nand(void);

static inline void wait_fifo_word(void)
{
    while ((CARD_CR2 & CARD_DATA_READY) == 0) {
    }
}

static inline u32 read_fifo_word(void)
{
    wait_fifo_word();
    return CARD_DATA_RD;
}

/* Outgoing FIFO words start one byte into an unaligned input stream. */
static inline u32 load_word(const u8 **address)
{
    const u8 *p = *address;
    u32 word = *p++;
    word |= (u32)*p++ << 8;
    word |= (u32)*p++ << 16;
    word |= (u32)*p++ << 24;
    *address = p;
    return word;
}

static inline bool odd_parity(u32 word)
{
    word ^= word >> 16;
    word ^= word >> 8;
    word ^= word >> 4;
    word ^= word >> 2;
    word &= 3;
    return word == 1 || word == 2;
}

static inline void finish_ecc(u8 *ecc, u32 all, u32 word_bit0, u32 word_bit1, u32 word_bit2,
                              u32 row, bool odd)
{
    word_bit2 ^= word_bit2 >> 16;
    word_bit1 ^= word_bit1 >> 16;
    word_bit0 ^= word_bit0 >> 16;
    word_bit2 ^= word_bit2 << 8;
    word_bit1 ^= word_bit1 >> 8;
    word_bit2 ^= word_bit2 >> 4;
    word_bit1 ^= word_bit1 << 4;
    word_bit0 ^= word_bit0 >> 8;
    word_bit2 ^= word_bit2 >> 2;
    word_bit1 ^= word_bit1 << 2;
    word_bit0 ^= word_bit0 << 4;
    u32 high_nibble = all & 0xf0f0f0f0;
    high_nibble ^= high_nibble << 16;
    word_bit0 ^= word_bit0 >> 2;
    high_nibble ^= high_nibble >> 8;
    u32 odd_bits = all & 0xaaaaaaaa;
    odd_bits ^= odd_bits << 16;
    u32 odd_bytes = (all & 0xff00ff00) ^ ((all & 0xff00ffff) >> 16);
    high_nibble ^= high_nibble << 2;
    odd_bits ^= odd_bits >> 8;
    u32 high_bytes = (all >> 24) ^ (all >> 16);
    u32 high_pairs = all & 0xcccccccc;
    high_pairs ^= high_pairs << 16;
    odd_bits ^= odd_bits >> 4;
    odd_bytes = (odd_bytes >> 12) ^ (odd_bytes >> 8);
    high_bytes ^= high_bytes >> 4;
    high_pairs ^= high_pairs >> 8;
    high_bytes ^= high_bytes << 2;
    odd_bytes ^= odd_bytes >> 2;
    high_pairs = (high_pairs ^ (high_pairs << 4)) >> 2;

    /* Pack the row and column parities before adding their complements. */
    u32 packed = ((high_bytes ^ (high_bytes << 1)) & 0x8) | ((odd_bytes ^ (odd_bytes << 1)) & 0x2) |
                 ((high_pairs ^ (high_pairs << 1)) & 0x200000) | ((row & 1) << 11) |
                 ((row & 4) << 13) | ((word_bit2 ^ (word_bit2 << 1)) & 0x200) |
                 ((word_bit1 ^ (word_bit1 << 1)) & 0x80) | ((word_bit0 ^ (word_bit0 << 1)) & 0x20) |
                 ((high_nibble ^ (high_nibble << 1)) & 0x800000) |
                 ((odd_bits ^ (odd_bits << 2)) & 0x80000) | ((row & 2) << 12) | ((row & 8) << 14);
    u32 complements = odd ? packed ^ 0xaaaaaa : packed;
    u32 code = ~(packed | (complements >> 1));
    ecc[0] = (u8)code;
    ecc[2] = (u8)(code >> 16);
    ecc[1] = (u8)(code >> 8);
}

NRIO_ROUTINE(0a5c) void nrio_read_nand_sector_with_ecc(u8 *buffer, u32 sector, u8 *ecc)
{
    nrio_send_nand_command(0x05);
    nrio_command_52(sector << 9);
    nrio_send_nand_command(0xe0);
    CARD_COMMAND[0] = 0xd0;
    CARD_COMMAND[1] = 0;
    CARD_CR2 = 0xa1000000;

    u32 all = 0, bit0 = 0, bit1 = 0, bit2 = 0, row = 0;
    bool odd = false;
    u32 *output = (u32 *)(buffer + ((u32)buffer & 3));
    for (u32 i = 0; i < 16; ++i) {
        u32 w0 = read_fifo_word();
        *output++ = w0;
        u32 w1 = read_fifo_word();
        *output++ = w1;
        u32 w2 = read_fifo_word();
        *output++ = w2;
        u32 w3 = read_fifo_word();
        *output++ = w3;
        u32 w4 = read_fifo_word();
        *output++ = w4;
        u32 w5 = read_fifo_word();
        *output++ = w5;
        u32 w6 = read_fifo_word();
        *output++ = w6;
        u32 w7 = read_fifo_word();
        *output++ = w7;

        u32 group = w0 ^ w1 ^ w2 ^ w3 ^ w4 ^ w5 ^ w6 ^ w7;
        bit0 ^= w1 ^ w3 ^ w5 ^ w7;
        bit1 ^= w2 ^ w3 ^ w6 ^ w7;
        bit2 ^= w4 ^ w5 ^ w6 ^ w7;
        all ^= group;
        if (odd_parity(group)) {
            row ^= i;
            odd = !odd;
        }
    }
    finish_ecc(ecc, all, bit0, bit1, bit2, row, odd);
}

NRIO_ROUTINE(0dc8) void nrio_write_nand_sector_with_ecc(const u8 *buffer, u32 sector, u8 *ecc)
{
    if ((nrio_nand_address & 0xfffc0000u) == 0) {
        return;
    }
    if (sector == 0) {
        nrio_send_nand_command(0x80);
        nrio_command_55(0);
    }

    const u8 *input = buffer;
    CARD_COMMAND[0] = 0xd3;
    CARD_COMMAND[1] = 0;
    CARD_COMMAND[7] = *input++;
    CARD_CR2 = 0xe1000008;

    u32 word = load_word(&input);
    const u32 *source = (const u32 *)(buffer + ((u32)buffer & 3));
    u32 all = 0, bit0 = 0, bit1 = 0, bit2 = 0, row = 0;
    bool odd = false;
    for (u32 i = 0; i < 16; ++i) {
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w0 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w1 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w2 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w3 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w4 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w5 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w6 = *source++;
        wait_fifo_word();
        CARD_DATA_RD = word;
        word = load_word(&input);
        u32 w7 = *source++;

        u32 group = w0 ^ w1 ^ w2 ^ w3 ^ w4 ^ w5 ^ w6 ^ w7;
        bit0 ^= w1 ^ w3 ^ w5 ^ w7;
        bit1 ^= w2 ^ w3 ^ w6 ^ w7;
        bit2 ^= w4 ^ w5 ^ w6 ^ w7;
        all ^= group;
        if (odd_parity(group)) {
            row ^= i;
            odd = !odd;
        }
    }
    finish_ecc(ecc, all, bit0, bit1, bit2, row, odd);
}
