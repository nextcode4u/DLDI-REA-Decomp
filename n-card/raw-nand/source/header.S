.section .nrio.raw.header, "a", %progbits
.global nrio_raw_header
nrio_raw_header:
    .word 0xbf8da5ed
    .asciz " Chishm"
    .byte 1, 13, 0x0c, 0        @ Version, image size, FIX_GOT | FIX_BSS, allocation
.ifdef RAW_NAND_WRITE
    .asciz "N-Card Raw R/W"
.else
    .asciz "N-Card Raw Read"
.endif
    .space 0x40 - (. - nrio_raw_header)
    .word nrio_raw_header, __bss_start
    .word __glue_start, __glue_start
    .word __got_start, __got_end
    .word __bss_start, __bss_end
    .ascii "NRWR"
.ifdef RAW_NAND_WRITE
    .word 0x23
.else
    .word 0x21
.endif
    .word nrio_raw_startup, nrio_raw_inserted
    .word nrio_raw_read, nrio_raw_write
    .word nrio_raw_clear, nrio_raw_shutdown
