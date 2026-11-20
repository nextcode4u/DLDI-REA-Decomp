#include "../ftl/state.h"
#include "nand_ecc.h"

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
