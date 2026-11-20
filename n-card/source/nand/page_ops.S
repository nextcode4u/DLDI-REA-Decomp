@ Spare-area validation, page operation dispatch, and physical block tables.
.syntax unified
.arm

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
