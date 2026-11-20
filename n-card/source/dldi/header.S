.section .nrio.header, "a", %progbits
.global nrio_header
nrio_header:
    .word 0xbf8da5ed
    .asciz " Chishm"
    .byte 1                  @ DLDI version
    .byte 15                 @ Reserved size is 32 KiB
    .byte 0x0e               @ Relocate glue and GOT, then clear BSS
    .byte 0                  @ Application allocation is filled by the patcher

    .asciz "NAND FLASH CARD LIBFATNRIO"
    .space 0x40 - (. - nrio_header)
    .word nrio_header
    .word __bss_start
    .word __glue_start
    .word __glue_start
    .word nrio_got
    .word __got_end
    .word __bss_start
    .word __bss_end

    .ascii "NRIO"
    .word 0x23               @ Slot-1, readable and writable
    .word nrio_startup
    .word nrio_is_inserted
    .word nrio_read_sectors
    .word nrio_write_sectors
    .word nrio_clear_status
    .word nrio_shutdown
