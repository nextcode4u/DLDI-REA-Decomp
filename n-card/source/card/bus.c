#include "../ftl/state.h"

/* NRIO sends NAND opcodes through the Slot-1 ROM command registers.
 * The CR1 high byte enables the card interface and completion IRQ.
 */
static inline u32 read_reply(void)
{
    while ((CARD_CR2 & CARD_DATA_READY) == 0) {
    }
    return CARD_DATA_RD;
}

NRIO_ROUTINE(013c) u32 nrio_card_probe(void)
{
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = 0x66;
    CARD_CR2 = 0xa7586000;
    return read_reply();
}

NRIO_ROUTINE(0180) u32 nrio_command_b7(u32 address)
{
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = 0xb7;
    CARD_COMMAND[1] = (u8)(address >> 24);
    CARD_COMMAND[2] = (u8)(address >> 16);
    CARD_COMMAND[3] = (u8)(address >> 8);
    CARD_COMMAND[4] = (u8)address;
    CARD_COMMAND[5] = 0;
    CARD_COMMAND[6] = 0;
    CARD_COMMAND[7] = 0;
    CARD_CR2 = 0xa7000000;
    return read_reply();
}

NRIO_ROUTINE(01fc) u32 nrio_command_87(u32 address, u16 value)
{
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = 0x87;
    CARD_COMMAND[1] = (u8)(address >> 24);
    CARD_COMMAND[2] = (u8)(address >> 16);
    CARD_COMMAND[3] = (u8)(address >> 8);
    CARD_COMMAND[4] = (u8)address;
    CARD_COMMAND[5] = (u8)(value >> 8);
    CARD_COMMAND[6] = (u8)value;
    CARD_COMMAND[7] = 0;
    CARD_CR2 = 0xa7000000;
    return read_reply();
}

NRIO_ROUTINE(0280) u32 nrio_read_reply(u8 command, u32 address, u16 value)
{
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = command;
    CARD_COMMAND[1] = (u8)(address >> 24);
    CARD_COMMAND[2] = (u8)(address >> 16);
    CARD_COMMAND[3] = (u8)(address >> 8);
    CARD_COMMAND[4] = (u8)address;
    CARD_COMMAND[5] = (u8)(value >> 8);
    CARD_COMMAND[6] = (u8)value;
    CARD_COMMAND[7] = 0;
    CARD_CR2 = 0xa7000000;
    return read_reply();
}

NRIO_ROUTINE(0300) void nrio_read_command(u8 b0, u8 b1, u8 b2, u8 b3, u8 b4,
                                    u8 b5, u8 b6, u8 b7, u32 *output, u8 length)
{
    u32 flags = length == 4 ? 0xa7180000 : 0xa1180000;
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = b0;
    CARD_COMMAND[1] = b1;
    CARD_COMMAND[2] = b2;
    CARD_COMMAND[3] = b3;
    CARD_COMMAND[4] = b4;
    CARD_COMMAND[5] = b5;
    CARD_COMMAND[6] = b6;
    CARD_COMMAND[7] = b7;
    CARD_CR2 = flags;

    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            *output++ = CARD_DATA_RD;
        }
    } while (CARD_CR2 & (u32)CARD_BUSY);
}

NRIO_ROUTINE(0390) int nrio_read_nand_id(u32 *output)
{
    nrio_read_command(0xd1, 0x61, 0x90, 0, 0, 0, 0, 0, output, 4);
    nrio_read_command(0xd1, 0x51, 0, 0, 0, 0, 0, 0, output, 4);
    nrio_read_command(0xd0, 0, 0, 0, 0, 0, 0, 0, output, 4);
    return 1;
}

NRIO_ROUTINE(0438) u32 nrio_command64(u32 first, u32 second)
{
    CARD_COMMAND[0] = (u8)(first >> 24);
    CARD_COMMAND[1] = (u8)(first >> 16);
    CARD_COMMAND[2] = (u8)(first >> 8);
    CARD_COMMAND[3] = (u8)first;
    CARD_COMMAND[4] = (u8)(second >> 24);
    CARD_COMMAND[5] = (u8)(second >> 16);
    CARD_COMMAND[6] = (u8)(second >> 8);
    CARD_COMMAND[7] = (u8)second;
    CARD_CR2 = 0xa7000000;
    return read_reply();
}

NRIO_ROUTINE(04c0) void nrio_command_address(u8 command, u32 address, u8 extra)
{
    CARD_COMMAND[0] = 0xd1;
    CARD_COMMAND[1] = command;
    CARD_COMMAND[2] = (u8)address;
    CARD_COMMAND[3] = (u8)(address >> 8);
    CARD_COMMAND[4] = (u8)(address >> 16);
    CARD_COMMAND[5] = (u8)(address >> 24);
    CARD_COMMAND[6] = extra;
    CARD_CR2 = 0xb7000001;
    (void)read_reply();
}

/* D1 61 prefixes a raw NAND opcode in the command frame. */
NRIO_ROUTINE(0534) void nrio_send_nand_command(u32 command)
{
    nrio_command64(0xd1610000 | (command << 8), 0);
}

/* C2 returns one ready bit in each byte of the card FIFO word. */
NRIO_ROUTINE(0548) void nrio_wait_nand(void)
{
    CARD_COMMAND[0] = 0xc2;
    do {
        CARD_CR2 = 0xa7000000;
    } while ((read_reply() & 0x80808080u) != 0x80808080u);
}

NRIO_ROUTINE(05a0) void nrio_command_52(u32 address)
{
    nrio_command_address(0x52, address, 0);
}

/* Preserve the column bits and carry the high row bits in the extra byte. */
NRIO_ROUTINE(05b0) void nrio_command_55(u32 page_count)
{
    u32 address = nrio_nand_address;
    u32 params = (address & 0x7ff) | ((address & 0xfffff800) << 5) | (page_count << 11);
    nrio_command_address(0x55, params, address >> 27);
}

NRIO_ROUTINE(05fc) void nrio_command_53(void)
{
    nrio_command_address(0x53, nrio_nand_address >> 11, 0);
}

NRIO_ROUTINE(0630) u8 nrio_read_status(void)
{
    nrio_command64(0xd1617000, 0);
    return nrio_command64(0xd0000000, 0);
}
static inline void begin_nand_data_read(void)
{
    CARD_COMMAND[0] = 0xd0;
    CARD_COMMAND[1] = 0;
}

NRIO_ROUTINE(0668) void nrio_read_nand_spare(u32 *output)
{
    nrio_send_nand_command(0x00);
    nrio_command_55(1);
    nrio_send_nand_command(0x30);
    nrio_wait_nand();

    begin_nand_data_read();
    u32 *last = output + 15;
    do {
        CARD_CR2 = 0xa7000000;
        *output = read_reply();
    } while (output++ != last);
}

NRIO_ROUTINE(06e0) void nrio_read_nand_sector(u32 *output, u32 sector_in_page)
{
    nrio_send_nand_command(0x05);
    nrio_command_52(sector_in_page << 9);
    nrio_send_nand_command(0xe0);

    begin_nand_data_read();
    CARD_CR2 = 0xa1000000;
    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            *output++ = CARD_DATA_RD;
        }
    } while ((s32)CARD_CR2 < 0);
}

NRIO_ROUTINE(0a08) void nrio_erase_nand_block(void)
{
    if ((nrio_nand_address & 0xfffc0000u) != 0) {
        nrio_send_nand_command(0x60);
        nrio_command_53();
        nrio_send_nand_command(0xd0);
        nrio_wait_nand();
    }
}

static inline u32 load_le32_and_advance(const u8 **address)
{
    const u8 *byte = *address;
    u32 word = *byte++;
    word |= (u32)*byte++ << 8;
    word |= (u32)*byte++ << 16;
    word |= (u32)*byte++ << 24;
    *address = byte;
    return word;
}

NRIO_ROUTINE(0908) void nrio_write_nand_sector_raw(const u8 *input, u32 sector)
{
    if ((nrio_nand_address & 0xfffc0000u) == 0) {
        return;
    }

    if (sector == 0) {
        nrio_send_nand_command(0x80);
        nrio_command_55(0);
    }

    CARD_COMMAND[0] = 0xd3;
    CARD_COMMAND[1] = 0;
    CARD_COMMAND[7] = *input++;
    CARD_CR2 = 0xe1000008;

    u32 word = load_le32_and_advance(&input);
    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            CARD_DATA_RD = word;
            word = load_le32_and_advance(&input);
        }
    } while ((s32)CARD_CR2 < 0);
}

NRIO_ROUTINE(0758) void nrio_write_nand_spare(const u32 *input)
{
    if ((nrio_nand_address & 0xfffc0000u) == 0) {
        return;
    }

    nrio_send_nand_command(0x80);
    nrio_command_55(1);

    const void *end = input + 16;
    /* Keep the post-incrementing load in the register used by the original. */
    register const u32 *cursor __asm__("lr") = input;
    __asm__ ("" : "+r"(cursor));

    CARD_COMMAND[0] = 0xd1;
    CARD_COMMAND[1] = 0x44;
    CARD_COMMAND[6] = 0;
    do {
        u32 word = *cursor++;
        CARD_COMMAND[2] = (u8)word;
        CARD_COMMAND[3] = (u8)(word >> 8);
        CARD_COMMAND[4] = (u8)(word >> 16);
        CARD_COMMAND[5] = (u8)(word >> 24);
        CARD_CR2 = 0xa7000000;
        (void)read_reply();
    } while (__builtin_expect((u32)cursor != (u32)end, 1));

    nrio_send_nand_command(0x10);
    nrio_wait_nand();
}
