#include "raw_nand.h"

enum {
    POLL_LIMIT = 2000000,
    NAND_POLL_LIMIT = 8192,
};

static bool wait_ready(void)
{
    for (u32 i = 0; i < POLL_LIMIT; ++i) {
        u32 status = CARD_CR2;
        if (status & CARD_DATA_READY) {
            return true;
        }
        if (!(status & (u32)CARD_BUSY)) {
            return false;
        }
    }
    return false;
}

static bool wait_idle(void)
{
    for (u32 i = 0; i < POLL_LIMIT; ++i) {
        if (!(CARD_CR2 & (u32)CARD_BUSY)) {
            return true;
        }
    }
    return false;
}

static bool receive_word(u32 *word)
{
    if (!wait_ready()) {
        return false;
    }
    *word = CARD_DATA_RD;
    return true;
}

static void frame(u8 first, u8 second, u32 address, u8 extra)
{
    CARD_COMMAND[0] = first;
    CARD_COMMAND[1] = second;
    CARD_COMMAND[2] = (u8)address;
    CARD_COMMAND[3] = (u8)(address >> 8);
    CARD_COMMAND[4] = (u8)(address >> 16);
    CARD_COMMAND[5] = (u8)(address >> 24);
    CARD_COMMAND[6] = extra;
    CARD_COMMAND[7] = 0;
}

static bool command_word(u32 control, u32 *reply)
{
    CARD_CR2 = control;
    return receive_word(reply) && wait_idle();
}

static bool opcode(u8 value)
{
    u32 reply;
    frame(0xd1, 0x61, value, 0);
    return command_word(0xa7000000, &reply);
}

static bool address_command(u8 command, u32 address, u8 extra)
{
    u32 reply;
    frame(0xd1, command, address, extra);
    return command_word(0xb7000001, &reply);
}

static bool wait_nand(void)
{
    u32 reply;
    CARD_COMMAND[0] = 0xc2;
    for (u32 i = 0; i < NAND_POLL_LIMIT; ++i) {
        if (!command_word(0xa7000000, &reply)) {
            return false;
        }
        if ((reply & 0x80808080u) == 0x80808080u) {
            return true;
        }
    }
    return false;
}

bool nand_read_id(u32 *id)
{
    CARD_CR1H = CARD_CR1_ENABLE | CARD_CR1_IRQ;
    if (!wait_idle()) {
        return false;
    }
    frame(0xd1, 0x61, 0x90, 0);
    if (!command_word(0xa7180000, id)) {
        return false;
    }
    frame(0xd1, 0x51, 0, 0);
    if (!command_word(0xa7180000, id)) {
        return false;
    }
    frame(0xd0, 0, 0, 0);
    return command_word(0xa7180000, id);
}

static bool page_address(u32 page, bool spare)
{
    return address_command(0x55, (page << 16) | (spare ? 0x800u : 0), (u8)(page >> 16));
}

bool nand_load_page(u32 page, bool spare)
{
    return opcode(0x00) && page_address(page, spare) && opcode(0x30) && wait_nand();
}

static void store_word(u8 *out, u32 word)
{
    out[0] = (u8)word;
    out[1] = (u8)(word >> 8);
    out[2] = (u8)(word >> 16);
    out[3] = (u8)(word >> 24);
}

bool nand_read_sector(u32 column, u8 *buffer)
{
    if (!opcode(0x05) || !address_command(0x52, column * SECTOR_SIZE, 0) || !opcode(0xe0)) {
        return false;
    }

    CARD_COMMAND[0] = 0xd0;
    CARD_COMMAND[1] = 0;
    CARD_CR2 = 0xa1000000;
    for (u32 i = 0; i < SECTOR_SIZE; i += 4) {
        u32 word;
        if (!receive_word(&word)) {
            return false;
        }
        store_word(buffer + i, word);
    }
    return wait_idle();
}

#ifdef RAW_NAND_WRITE
bool nand_read_spare(u8 *buffer)
{
    CARD_COMMAND[0] = 0xd0;
    CARD_COMMAND[1] = 0;
    for (u32 i = 0; i < SPARE_SIZE; i += 4) {
        u32 word;
        if (!command_word(0xa7000000, &word)) {
            return false;
        }
        store_word(buffer + i, word);
    }
    return true;
}

static bool successful_status(void)
{
    u32 reply;
    if (!wait_nand() || !opcode(0x70)) {
        return false;
    }
    frame(0xd0, 0, 0, 0);
    return command_word(0xa7000000, &reply) && (reply & 0xc1) == 0xc0;
}

bool nand_erase_block(u32 page)
{
    return opcode(0x60) && address_command(0x53, page, 0) &&
           opcode(0xd0) && successful_status();
}

static bool send_sector(const u8 *data)
{
    CARD_COMMAND[0] = 0xd3;
    CARD_COMMAND[1] = 0;
    CARD_COMMAND[7] = data[0];
    CARD_CR2 = 0xe1000008;
    for (u32 i = 1; i <= SECTOR_SIZE; i += 4) {
        if (!wait_ready()) {
            return false;
        }
        // The bridge takes the first byte in the frame and then 128 FIFO words.
        // The last word uses one lookahead byte from the page record.
        CARD_DATA_RD = (u32)data[i] | ((u32)data[i + 1] << 8) |
                      ((u32)data[i + 2] << 16) | ((u32)data[i + 3] << 24);
    }
    return wait_idle();
}

bool nand_program_page(u32 page, const u8 *record)
{
    if (!opcode(0x80) || !page_address(page, false)) {
        return false;
    }
    for (u32 i = 0; i < PAGE_SIZE; i += SECTOR_SIZE) {
        if (!send_sector(record + i)) {
            return false;
        }
    }
    CARD_COMMAND[0] = 0xd1;
    CARD_COMMAND[1] = 0x44;
    CARD_COMMAND[6] = 0;
    for (u32 i = PAGE_SIZE; i < PAGE_RECORD_SIZE; i += 4) {
        u32 reply;
        for (u32 j = 0; j < 4; ++j) {
            CARD_COMMAND[2 + j] = record[i + j];
        }
        if (!command_word(0xa7000000, &reply)) {
            return false;
        }
    }
    return opcode(0x10) && successful_status();
}
#endif
