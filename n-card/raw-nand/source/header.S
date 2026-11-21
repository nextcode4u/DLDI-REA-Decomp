.section .nrio.raw.header, "a", %progbits
.global nrio_raw_header
nrio_raw_header:
    .word 0xbf8da5ed
    .asciz " Chishm"
.global nrio_raw_allocation
    .set nrio_raw_allocation, 16384
    .byte 1, 14, 0x0c, 0        @ Version, image size, FIX_GOT | FIX_BSS, allocation
    .asciz "N-Card Recovery Toolkit"
    .space 0x40 - (. - nrio_raw_header)
    .word nrio_raw_header, __bss_start
    .word __glue_start, __glue_start
    .word __got_start, __got_end
    .word __bss_start, __bss_end
    .ascii "NRWR"
    .word 0x21               @ Standard DLDI callers start with read-only access.
    .word nrio_raw_startup, nrio_raw_inserted
    .word nrio_raw_read, nrio_raw_write
    .word nrio_raw_clear, nrio_raw_shutdown
