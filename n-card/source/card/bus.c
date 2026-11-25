#include "../ftl/state.h"

#ifdef NRIO_UNIFIED
enum { CARD_POLL_LIMIT = 1 << 20, NAND_READY_LIMIT = 8192 };

static bool wait_card_data(u32 *remaining)
{
    if (nrio_bus_fault) return false;
    while (*remaining) {
        --*remaining;
        u32 status = CARD_CR2;
        if (status & CARD_DATA_READY) return true;
        if (!(status & (u32)CARD_BUSY)) break;
    }
    nrio_latch_bus_fault();
    return false;
}

bool nrio_wait_card_data(void)
{
    u32 budget = CARD_POLL_LIMIT;
    return wait_card_data(&budget);
}

bool nrio_wait_card_idle(void)
{
    if (nrio_bus_fault) return false;
    for (u32 n = 0; n < CARD_POLL_LIMIT; ++n)
        if (!(CARD_CR2 & (u32)CARD_BUSY)) return true;
    nrio_latch_bus_fault();
    return false;
}
#endif

/* NRIO sends NAND opcodes through the Slot-1 ROM command registers.
 * The CR1 high byte enables the card interface and completion IRQ.
 */
static inline u32 read_reply(void)
{
#ifdef NRIO_UNIFIED
    if (!nrio_wait_card_data()) return 0;
#else
    while ((CARD_CR2 & CARD_DATA_READY) == 0) {
    }
#endif
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
#ifdef NRIO_UNIFIED
    for (unsigned i = 0; i < (length == 4 ? 1u : 128u); ++i) {
        if (!nrio_wait_card_data()) return;
        *output++ = CARD_DATA_RD;
    }
    (void)nrio_wait_card_idle();
#else
    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            *output++ = CARD_DATA_RD;
        }
    } while (CARD_CR2 & (u32)CARD_BUSY);
#endif
}

#ifdef NRIO_EXPERIMENTAL
static bool probe_word(u8 b0, u8 b1, u8 b2, u32 *out)
{
    enum { LIMIT = 1 << 20 };
    for (u32 n = 0; CARD_CR2 & (u32)CARD_BUSY; ++n)
        if (n == LIMIT) return false;
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    CARD_COMMAND[0] = b0;
    CARD_COMMAND[1] = b1;
    CARD_COMMAND[2] = b2;
    for (unsigned i = 3; i < 8; ++i) CARD_COMMAND[i] = 0;
    CARD_CR2 = 0xa7180000;
    bool received = false;
    for (u32 n = 0; n < LIMIT; ++n) {
        u32 status = CARD_CR2;
        if (status & CARD_DATA_READY) {
            *out = CARD_DATA_RD;
            received = true;
        }
        if (!(status & (u32)CARD_BUSY)) return received;
    }
    return false;
}
#endif

NRIO_ROUTINE(0390) int nrio_read_nand_id(u32 *output)
{
#ifdef NRIO_EXPERIMENTAL
    return probe_word(0xd1, 0x61, 0x90, output) &&
           probe_word(0xd1, 0x51, 0, output) &&
           probe_word(0xd0, 0, 0, output);
#else
    nrio_read_command(0xd1, 0x61, 0x90, 0, 0, 0, 0, 0, output, 4);
    nrio_read_command(0xd1, 0x51, 0, 0, 0, 0, 0, 0, output, 4);
    nrio_read_command(0xd0, 0, 0, 0, 0, 0, 0, 0, output, 4);
    return 1;
#endif
}

NRIO_ROUTINE(0438) u32 nrio_command64(u32 first, u32 second)
{
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return 0;
#endif
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
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif
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
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
    u32 budget = CARD_POLL_LIMIT;
    CARD_COMMAND[0] = 0xc2;
    for (unsigned i = 0; i < NAND_READY_LIMIT; ++i) {
        CARD_CR2 = 0xa7000000;
        if (!wait_card_data(&budget)) return;
        if ((CARD_DATA_RD & 0x80808080u) == 0x80808080u) return;
    }
    nrio_latch_bus_fault();
#else
    CARD_COMMAND[0] = 0xc2;
    do {
        CARD_CR2 = 0xa7000000;
    } while ((read_reply() & 0x80808080u) != 0x80808080u);
#endif
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
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif

    begin_nand_data_read();
    u32 *last = output + 15;
    do {
        CARD_CR2 = 0xa7000000;
        *output = read_reply();
#ifdef NRIO_UNIFIED
        if (nrio_bus_fault) return;
#endif
    } while (output++ != last);
}

NRIO_ROUTINE(06e0) void nrio_read_nand_sector(u32 *output, u32 sector_in_page)
{
    nrio_send_nand_command(0x05);
    nrio_command_52(sector_in_page << 9);
    nrio_send_nand_command(0xe0);
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif

    begin_nand_data_read();
    CARD_CR2 = 0xa1000000;
#ifdef NRIO_UNIFIED
    for (unsigned i = 0; i < 128; ++i) {
        if (!nrio_wait_card_data()) return;
        *output++ = CARD_DATA_RD;
    }
    (void)nrio_wait_card_idle();
#else
    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            *output++ = CARD_DATA_RD;
        }
    } while ((s32)CARD_CR2 < 0);
#endif
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

#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif
    CARD_COMMAND[0] = 0xd3;
    CARD_COMMAND[1] = 0;
    CARD_COMMAND[7] = *input++;
    CARD_CR2 = 0xe1000008;
#ifdef NRIO_UNIFIED
    for (unsigned i = 0; i < 127; ++i) {
        if (!nrio_wait_card_data()) return;
        CARD_DATA_RD = load_le32_and_advance(&input);
    }
    if (!nrio_wait_card_data()) return;
    CARD_DATA_RD = input[0] | ((u32)input[1] << 8) | ((u32)input[2] << 16) | 0xff000000u;
    (void)nrio_wait_card_idle();
#else
    u32 word = load_le32_and_advance(&input);
    do {
        if (CARD_CR2 & CARD_DATA_READY) {
            CARD_DATA_RD = word;
            word = load_le32_and_advance(&input);
        }
    } while ((s32)CARD_CR2 < 0);
#endif
}

NRIO_ROUTINE(0758) void nrio_write_nand_spare(const u32 *input)
{
    if ((nrio_nand_address & 0xfffc0000u) == 0) {
        return;
    }

    nrio_send_nand_command(0x80);
    nrio_command_55(1);
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif

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
#ifdef NRIO_UNIFIED
        if (nrio_bus_fault) return;
#endif
    } while (__builtin_expect((u32)cursor != (u32)end, 1));

    nrio_send_nand_command(0x10);
    nrio_wait_nand();
}

NRIO_ROUTINE(0838) void nrio_finish_nand_write(const u32 *input)
{
    /* Match GCC's register assignments for the input and FIFO cursor. */
#ifdef NRIO_UNIFIED
    if (nrio_bus_fault) return;
#endif
    register u32 start __asm__("ip") = (u32)input;
    __asm__ ("" : "+r"(start));
    if ((nrio_nand_address & 0xfffc0000u) == 0) {
        return;
    }
    CARD_COMMAND[0] = 0xd1;
    register const u32 *cursor __asm__("lr") = (const u32 *)start;
    /* Keep cursor setup ahead of the second command byte. */
    __asm__ ("" : "+r"(cursor) : "r"(0x44), "m"(CARD_COMMAND[1]));
    CARD_COMMAND[1] = 0x44;
    CARD_COMMAND[6] = 0;
    const void *end = cursor + 16;
    do {
        u32 word = *cursor++;
        CARD_COMMAND[2] = (u8)word;
        CARD_COMMAND[3] = (u8)(word >> 8);
        CARD_COMMAND[4] = (u8)(word >> 16);
        CARD_COMMAND[5] = (u8)(word >> 24);
        CARD_CR2 = 0xa7000000;
        (void)read_reply();
#ifdef NRIO_UNIFIED
        if (nrio_bus_fault) return;
#endif
    } while (__builtin_expect((u32)cursor != (u32)end, 1));
    nrio_send_nand_command(0x10);
    nrio_wait_nand();
}
