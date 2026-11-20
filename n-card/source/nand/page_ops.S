@ Spare-area validation, page operation dispatch, and physical block tables.
.syntax unified
.arm

.section .nrio.1694, "ax", %progbits

.global nrio_check_spare_markers
.type nrio_check_spare_markers, %function
nrio_check_spare_markers:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_1698:
    ldr     sl, .L180c
    ldr     r0, .L1810
    add     sl, pc, sl
    ldr     r2, [sl, r0]
    ldrb    r3, [r2]
    cmp     r3, #0xff
    sub     sp, sp, #12
    beq     .L17d0
    cmp     r3, #0x55
    beq     .L16e0
.L16c0:
    ldr     r3, [sl, r0]
    ldrb    r2, [r3, #2]
    cmp     r2, #0x55
    beq     .L16e0
.L16d0:
    mov     r0, #0xfe
.L16d4:
    add     sp, sp, #12
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L16e0:
    ldr     r2, [sl, r0]
    ldrb    r3, [r2, #1]
    cmp     r3, #0xaa
    beq     .L16fc
    ldrb    r3, [r2, #3]
    cmp     r3, #0xaa
    bne     .L16d0
.L16fc:
    ldrb    r1, [r2, #15]
    ldrb    r3, [r2, #11]
    cmp     r3, r1
    beq     .L1718
    ldrb    r3, [r2, #7]
    cmp     r3, r1
    strbeq  r3, [r2, #11]
.L1718:
    ldr     r2, [sl, r0]
    ldrb    r3, [r2, #11]
    cmp     r3, #0xff
    beq     .L16d0
    mov     r8, r2
    mov     r7, r2
    mov     r1, #0
    mov     r9, r2
.L1738:
    orr     r0, r1, #10
    add     r3, r1, #16
    orr     lr, r1, #8
    orr     r2, r1, #12
    orr     r5, r1, #9
    str     r0, [sp, #4]
    orr     ip, r1, #13
    orr     r6, r1, #14
    orr     r0, r1, #4
    orr     r4, r1, #5
    orr     fp, r1, #6
    ldrb    r2, [r8, r2]
    and     r1, r3, #0xff
    ldrb    r3, [r8, lr]
    cmp     r3, r2
    beq     .L1784
    ldrb    r3, [r8, r0]
    cmp     r3, r2
    strbeq  r3, [r8, lr]
.L1784:
    ldrb    r2, [r7, ip]
    ldrb    r3, [r7, r5]
    cmp     r3, r2
    beq     .L17a0
    ldrb    r3, [r7, r4]
    cmp     r3, r2
    strbeq  r3, [r7, r5]
.L17a0:
    ldr     r0, [sp, #4]
    ldrb    r2, [r9, r6]
    ldrb    r3, [r9, r0]
    cmp     r3, r2
    beq     .L17c0
    ldrb    r3, [r9, fp]
    cmp     r3, r2
    strbeq  r3, [r9, r0]
.L17c0:
    cmp     r1, #0x40
    bne     .L1738
    mov     r0, #0
    b       .L16d4
.L17d0:
    ldrb    r3, [r2, #2]
    cmp     r3, #0xff
    bne     .L16c0
    ldrb    r1, [r2, #15]
    ldrb    r3, [r2, #11]
    cmp     r3, r1
    beq     .L17f8
    ldrb    r3, [r2, #7]
    cmp     r3, r1
    strbeq  r3, [r2, #11]
.L17f8:
    ldr     r3, [sl, r0]
    ldrb    r0, [r3, #11]
    cmp     r0, #0xff
    bne     .L16d0
    b       .L16d4
.L180c:
    .word   nrio_got - (.Lgotpc_1698 + 16)
.L1810:
    .word   0x00000074

.section .nrio.1888, "ax", %progbits

.section .nrio.195c, "ax", %progbits

.global nrio_page_operation
.type nrio_page_operation, %function
nrio_page_operation:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_1960:
    ldr     sl, .L1ca0
    sub     r0, r0, #1
    add     sl, pc, sl
    sub     sp, sp, #12
    cmp     r0, #10
    addls   pc, pc, r0, lsl #2
    b       .L1a30
    b       .L1aa4
    b       .L1a3c
    b       .L1c54
    b       .L1be8
    b       .L1bb8
    b       .L1b84
    b       .L1b7c
    b       .L1ab4
    b       .L19a8
    b       .L1aa4
    b       .L1a84
.L19a8:
    ldr     r0, .L1ca4
    ldr     fp, .L1ca8
    ldr     r3, [sl, r0]
    ldr     r1, [sl, fp]
    ldrb    r2, [r3]
    add     r3, r1, #0x200
    lsl     r2, r2, #9
    add     r3, r2, r3
    add     r2, r2, r1
    cmp     r2, r3
    bcs     .L19e4
    mvn     r1, #0
.L19d8:
    str     r1, [r2], #4
    cmp     r3, r2
    bhi     .L19d8
.L19e4:
    ldr     r4, [sl, r0]
    ldr     r0, [sl, fp]
    ldrb    r1, [r4]
    add     r0, r0, r1, lsl #9
    bl      nrio_write_nand_sector_raw
    ldr     r3, .L1cac
    ldrb    r1, [r4]
    ldr     r2, [sl, r3]
    mvn     r3, #0
    add     r2, r2, r1, lsl #4
    strb    r3, [r2, #6]
    strb    r3, [r2, #8]
    strb    r3, [r2, #12]
    strb    r3, [r2, #4]
    strb    r3, [r2, #9]
    strb    r3, [r2, #13]
    strb    r3, [r2, #5]
    strb    r3, [r2, #10]
    strb    r3, [r2, #14]
.L1a30:
    add     sp, sp, #12
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L1a3c:
    ldr     r3, .L1cb0
    ldr     r2, .L1ca4
    ldr     r6, [sl, r3]
    ldr     r4, [sl, r2]
    add     r5, sp, #4
    ldr     r0, [r6]
    ldrb    r1, [r4]
    mov     r2, r5
    bl      nrio_read_nand_sector_with_ecc
    ldr     r0, .L1cac
    ldr     r0, [sl, r0]
    ldrb    r3, [r4]
    add     r0, r0, #8
    add     r0, r0, r3, lsl #4
    mov     r1, r5
    ldr     r2, [r6]
    bl      nrio_correct_ecc
    b       .L1a30
.L1a84:
    ldr     r3, .L1cb0
    ldr     r2, [sl, r3]
    ldr     r3, .L1ca4
    ldr     r0, [r2]
    ldr     r2, [sl, r3]
    ldrb    r1, [r2]
    bl      nrio_read_nand_sector
    b       .L1a30
.L1aa4:
    ldr     r3, .L1cac
    ldr     r0, [sl, r3]
    bl      nrio_read_nand_spare
    b       .L1a30
.L1ab4:
    ldr     fp, .L1ca8
    ldr     r3, [sl, fp]
    add     r1, r3, #0x200
    cmp     r3, r1
    bcs     .L1ad8
    mvn     r2, #0
.L1acc:
    str     r2, [r3], #4
    cmp     r3, r1
    bcc     .L1acc
.L1ad8:
    ldr     r3, .L1cac
    ldr     r9, [sl, r3]
    mov     r7, #0
    mvn     r8, #0
.L1ae8:
    orr     r3, r7, #8
    orr     r2, r7, #12
    orr     r1, r7, #4
    orr     r0, r7, #9
    orr     ip, r7, #13
    orr     lr, r7, #5
    orr     r4, r7, #10
    orr     r5, r7, #14
    orr     r6, r7, #6
    add     r7, r7, #16
    cmp     r7, #0x40
    strb    r8, [r9, r3]
    strb    r8, [r9, r2]
    strb    r8, [r9, r1]
    strb    r8, [r9, r0]
    strb    r8, [r9, ip]
    strb    r8, [r9, lr]
    strb    r8, [r9, r4]
    strb    r8, [r9, r5]
    strb    r8, [r9, r6]
    bne     .L1ae8
    ldr     r4, [sl, fp]
    mov     r1, #0
    mov     r0, r4
    bl      nrio_write_nand_sector_raw
    mov     r0, r4
    mov     r1, #1
    bl      nrio_write_nand_sector_raw
    mov     r0, r4
    mov     r1, #2
    bl      nrio_write_nand_sector_raw
    mov     r0, r4
    mov     r1, #3
    bl      nrio_write_nand_sector_raw
    mov     r0, r9
    bl      nrio_finish_nand_write
    b       .L1a30
.L1b7c:
    bl      nrio_erase_nand_block
    b       .L1a30
.L1b84:
    ldr     r3, .L1ca4
    ldr     r2, .L1cac
    ldr     r1, [sl, r3]
    ldr     r3, .L1ca8
    ldr     r2, [sl, r2]
    ldrb    ip, [r1]
    ldr     r0, [sl, r3]
    add     r2, r2, #8
    mov     r1, ip
    add     r0, r0, ip, lsl #9
    add     r2, r2, ip, lsl #4
    bl      nrio_write_nand_sector_with_ecc
    b       .L1a30
.L1bb8:
    ldr     r3, .L1ca4
    ldr     r2, .L1cac
    ldr     r0, [sl, r3]
    ldr     r3, .L1cb4
    ldr     r2, [sl, r2]
    ldrb    r1, [r0]
    ldr     ip, [sl, r3]
    add     r2, r2, #8
    ldr     r0, [ip]
    add     r2, r2, r1, lsl #4
    bl      nrio_write_nand_sector_with_ecc
    b       .L1a30
.L1be8:
    ldr     r3, .L1cac
    ldr     lr, [sl, r3]
    mov     ip, #0
.L1bf4:
    orr     r3, ip, #8
    ldrb    r0, [lr, r3]
    orr     r2, ip, #12
    orr     r1, ip, #4
    strb    r0, [lr, r2]
    strb    r0, [lr, r1]
    orr     r3, ip, #9
    ldrb    r0, [lr, r3]
    orr     r2, ip, #13
    orr     r1, ip, #5
    strb    r0, [lr, r2]
    strb    r0, [lr, r1]
    orr     r3, ip, #10
    ldrb    r2, [lr, r3]
    orr     r1, ip, #14
    orr     r3, ip, #6
    add     ip, ip, #16
    cmp     ip, #0x40
    strb    r2, [lr, r1]
    strb    r2, [lr, r3]
    bne     .L1bf4
    mov     r0, lr
    bl      nrio_finish_nand_write
    b       .L1a30
.L1c54:
    ldr     r3, .L1ca4
    ldr     r2, .L1ca8
    ldr     r4, [sl, r3]
    ldr     r5, [sl, r2]
    ldrb    r0, [r4]
    add     r6, sp, #4
    mov     r1, r0
    mov     r2, r6
    add     r0, r5, r0, lsl #9
    bl      nrio_read_nand_sector_with_ecc
    ldr     r0, .L1cac
    ldr     r0, [sl, r0]
    ldrb    r3, [r4]
    add     r0, r0, #8
    add     r2, r5, r3, lsl #9
    add     r0, r0, r3, lsl #4
    mov     r1, r6
    bl      nrio_correct_ecc
    b       .L1a30
.L1ca0:
    .word   nrio_got - (.Lgotpc_1960 + 16)
.L1ca4:
    .word   0x000000d4
.L1ca8:
    .word   0x000000ac
.L1cac:
    .word   0x00000074
.L1cb0:
    .word   0x000000a4
.L1cb4:
    .word   0x00000018

.global nrio_load_physical_block_table
.type nrio_load_physical_block_table, %function
nrio_load_physical_block_table:
    push    {r4, r5, r6, r7, r8, r9, sl, lr}
.Lgotpc_1cbc:
    ldr     sl, .L1da0
    ldr     r3, .L1da4
    add     sl, pc, sl
    ldr     r4, [sl, r3]
    ldr     r3, .L1da8
    ldr     r2, [sl, r3]
    ldrh    r0, [r4]
    ldr     r1, [r2]
    lsr     r0, r0, #4
    add     r0, r1, r0, lsl #11
    bl      nrio_set_nand_address
    mov     r0, #1
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    subs    r9, r0, #0
    bne     .L1d10
    ldr     r3, .L1dac
    ldr     r2, [sl, r3]
    ldrb    r1, [r2, #11]
    cmp     r1, #0xab
    beq     .L1d18
.L1d10:
    pop     {r4, r5, r6, r7, r8, r9, sl, lr}
    bx      lr
.L1d18:
    ldrh    r3, [r4]
    ldr     r2, .L1db0
    and     r3, r3, #15
    ldr     r1, [sl, r2]
    asr     r3, r3, #2
    strb    r3, [r1]
    add     r0, r0, #3
    bl      nrio_page_operation
    ldr     r3, .L1db4
    ldr     r2, [sl, r3]
    ldr     r3, .L1db8
    ldr     r8, [r2]
    ldr     r7, [sl, r3]
    ldr     r2, .L1dbc
    ldr     r3, .L1dc0
    ldrh    r1, [r4]
    ldr     r6, [sl, r2]
    ldr     r2, [sl, r3]
    and     r1, r1, #15
    add     r5, r2, r1, lsl #7
    mov     r4, r9
.L1d6c:
    ldrh    r3, [r4, r5]
    ldr     r2, [r7]
    lsl     r0, r3, #8
    ldrb    r1, [r6]
    orr     r0, r0, r3, lsr #8
    rsb     r0, r2, r0, lsl #16
    lsl     r1, r1, #11
    bl      __udivsi3
    str     r0, [r8, r4, lsl #1]
    add     r4, r4, #2
    cmp     r4, #0x80
    bne     .L1d6c
    b       .L1d10
.L1da0:
    .word   nrio_got - (.Lgotpc_1cbc + 16)
.L1da4:
    .word   0x00000040
.L1da8:
    .word   0x000000f8
.L1dac:
    .word   0x00000074
.L1db0:
    .word   0x000000d4
.L1db4:
    .word   0x00000028
.L1db8:
    .word   0x00000058
.L1dbc:
    .word   0x00000068
.L1dc0:
    .word   0x000000ac

.global nrio_find_block_table
.type nrio_find_block_table, %function
nrio_find_block_table:
    push    {r4, r5, r6, r7, sl, lr}
.Lgotpc_1dc8:
    ldr     sl, .L1e54
    ldr     r6, .L1e58
    add     sl, pc, sl
    ldr     r3, [sl, r6]
    ldr     r1, [r3]
    ldr     r2, .L1e5c
    ldr     r3, .L1e60
    ldr     r5, [sl, r2]
    ldr     r7, [sl, r3]
    sub     r4, r1, #0x100000
    b       .L1e20
.L1df4:
    bl      nrio_set_nand_address
    mov     r0, #10
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0
    bne     .L1e18
    ldrb    r3, [r7, #11]
    cmp     r3, #0xab
    beq     .L1e40
.L1e18:
    ldrb    r3, [r5]
    add     r4, r4, r3, lsl #11
.L1e20:
    ldr     r3, [sl, r6]
    ldr     r2, [r3]
    cmp     r4, r2
    mov     r0, r4
    bne     .L1df4
    mov     r0, #0
.L1e38:
    pop     {r4, r5, r6, r7, sl, lr}
    bx      lr
.L1e40:
    ldr     r3, .L1e64
    ldr     r2, [sl, r3]
    mov     r0, #1
    str     r4, [r2]
    b       .L1e38
.L1e54:
    .word   nrio_got - (.Lgotpc_1dc8 + 16)
.L1e58:
    .word   0x00000058
.L1e5c:
    .word   0x00000068
.L1e60:
    .word   0x00000074
.L1e64:
    .word   0x000000f8

.section .nrio.1ec8, "ax", %progbits

.global nrio_copy_nand_pages
.type nrio_copy_nand_pages, %function
nrio_copy_nand_pages:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_1ecc:
    ldr     sl, .L1ff4
    cmp     r0, #0xff
    add     sl, pc, sl
    mov     r8, r1
    sub     sp, sp, #12
    mov     r7, r0
    mov     r1, r2
    beq     .L1fe4
    cmp     r2, r3
    bcs     .L1fe4
    ldr     r2, .L1ff8
    str     r2, [sp, #4]
    rsb     r2, r1, r3
    ldr     r3, [sp, #4]
    ldr     fp, [sl, r3]
    ldr     r3, .L1ffc
    add     r2, r1, r2
    ldr     r9, [sl, r3]
    str     fp, [sp]
    mov     r5, r1
    and     r6, r2, #0xff
.L1f20:
    mov     r1, r5
    mov     r0, r7
    bl      nrio_select_physical_page
    mov     r0, #1
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0xff
    beq     .L1fd4
    ldr     r3, [sp, #4]
    ldr     r2, [sl, r3]
    mov     r3, #0
    strb    r3, [r2]
    mov     r4, fp
.L1f54:
    mov     r0, #3
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L1f54
    mov     r0, r8
    mov     r1, r5
    bl      nrio_select_physical_page
    mov     r3, #0
    strb    r3, [r4]
    ldr     r4, [sp]
.L1f8c:
    mov     r0, #6
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L1f8c
    ldr     r3, .L2000
    ldr     r2, [sl, r3]
    ldrb    r1, [r2]
    strb    r1, [r9, #11]
    bl      nrio_set_spare_markers
    mov     r0, #4
    bl      nrio_page_operation
    bl      nrio_decode_nand_status
    cmp     r0, #0
    bne     .L1fe8
.L1fd4:
    add     r3, r5, #1
    and     r5, r3, #0xff
    cmp     r5, r6
    bne     .L1f20
.L1fe4:
    mov     r0, #0
.L1fe8:
    add     sp, sp, #12
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L1ff4:
    .word   nrio_got - (.Lgotpc_1ecc + 16)
.L1ff8:
    .word   0x000000d4
.L1ffc:
    .word   0x00000074
.L2000:
    .word   0x000000ec

.section .nrio.2114, "ax", %progbits

.global nrio_classify_physical_block
.type nrio_classify_physical_block, %function
nrio_classify_physical_block:
    push    {r4, r5, r6, r7, r8, r9, sl, lr}
.Lgotpc_2118:
    ldr     sl, .L2210
    ldr     r3, .L2214
    add     sl, pc, sl
    ldr     r7, [sl, r3]
    ldr     r2, .L2218
    ldr     r3, .L221c
    ldr     r8, [sl, r2]
    ldr     r6, [sl, r3]
    mov     r5, r0
    mov     r4, #2
.L2140:
    mov     r1, #0
    mov     r0, r5
    bl      nrio_select_physical_page
    mov     r0, #10
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0xfe
    mov     r0, r5
    bne     .L2180
.L2164:
    sub     r3, r4, #1
    and     r4, r3, #0xff
    cmp     r4, #0xff
    bne     .L2140
.L2174:
    mov     r0, #0xfe
.L2178:
    pop     {r4, r5, r6, r7, r8, r9, sl, lr}
    bx      lr
.L2180:
    ldrb    r1, [r6]
    ldrb    r9, [r7, #11]
    bl      nrio_select_physical_page
    mov     r0, #10
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0xfe
    beq     .L2164
    ldrb    r3, [r7, #11]
    cmp     r9, r3
    beq     .L21d8
    cmp     r3, #0xff
    bne     .L2174
    ldrb    r3, [r8]
    cmp     r3, #0xff
    beq     .L21f8
    mov     r0, r5
    bl      nrio_retry_block_erase
    cmp     r0, #0
    bne     .L2174
.L21d0:
    mov     r0, #0xff
    b       .L2178
.L21d8:
    cmp     r9, #0xff
    beq     .L21d0
    ldr     r3, .L2220
    ldr     r2, [sl, r3]
    ldr     r1, [r2]
    mov     r0, #0
    strb    r5, [r9, r1]
    b       .L2178
.L21f8:
    ldr     r3, .L2224
    ldr     r2, [sl, r3]
    mov     r0, #0xfa
    strb    r9, [r2]
    strb    r5, [r8]
    b       .L2178
.L2210:
    .word   nrio_got - (.Lgotpc_2118 + 16)
.L2214:
    .word   0x00000074
.L2218:
    .word   0x000000e4
.L221c:
    .word   0x000000bc
.L2220:
    .word   0x000000b8
.L2224:
    .word   0x000000ec
