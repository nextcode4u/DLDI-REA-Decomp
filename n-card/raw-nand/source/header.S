.section .nrio.raw.header, "a", %progbits
.global nrio_raw_header
nrio_raw_header:
    .word 0xbf8da5ed
    .asciz " Chishm"
    .byte 1, 13, 0x0c, 0        @ Version, image size, FIX_GOT | FIX_BSS, allocation
.ifdef RAW_NAND_TOOLKIT
    .asciz "N-Card Recovery Toolkit"
.else
.ifdef RAW_NAND_WRITE
.ifdef RAW_NAND_SPARE
    .asciz "N-Card Spare RW"
.else
.ifdef RAW_NAND_PROTECT_BOOT
    .asciz "N-Card Guarded"
.else
.ifdef RAW_NAND_BANNER
    .asciz "N-Card Banner"
.else
    .asciz "N-Card Raw R/W"
.endif
.endif
.endif
.else
.ifdef RAW_NAND_SPARE
    .asciz "N-Card Spare"
.else
    .asciz "N-Card Raw Read"
.endif
.endif
.endif
    .space 0x40 - (. - nrio_raw_header)
    .word nrio_raw_header, __bss_start
    .word __glue_start, __glue_start
    .word __got_start, __got_end
    .word __bss_start, __bss_end
    .ascii "NRWR"
.ifdef RAW_NAND_TOOLKIT
    .word 0x21               @ Standard DLDI callers start with read-only access.
.else
.ifdef RAW_NAND_WRITE
    .word 0x23
.else
    .word 0x21
.endif
.endif
    .word nrio_raw_startup, nrio_raw_inserted
    .word nrio_raw_read, nrio_raw_write
    .word nrio_raw_clear, nrio_raw_shutdown
