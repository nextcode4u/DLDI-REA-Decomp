@ Sector reads and initialization of the NAND geometry and block maps.
.syntax unified
.arm
.section .nrio.30ec, "ax", %progbits

.global nrio_ftl_read
.type nrio_ftl_read, %function
nrio_ftl_read:
    push    {r4, r5, r6, r7, r8, sl, lr}
.Lgotpc_30f0:
    ldr     sl, .L3190
    ldr     r3, .L3194
    add     sl, pc, sl
    ldr     ip, [sl, r3]
    ldr     r3, .L3198
    ldr     r5, [sl, r3]
    ldr     r3, .L319c
    lsr     r0, r0, #9
    ldr     lr, [ip]
    lsr     r6, r2, #9
    add     ip, r0, #1
    ldr     r2, [sl, r3]
    ldr     r3, .L31a0
    cmp     ip, lr
    mov     ip, #1
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    sub     sp, sp, #4
    str     r6, [r2]
    mov     r4, r1
    str     r0, [r5]
    bhi     .L3184
    cmp     r6, #0
    beq     .L3184
    ldr     r3, .L31a4
    ldr     r8, [sl, r3]
    mov     r7, r5
    mov     r5, #0
.L3160:
    str     r4, [r8]
    bl      nrio_read_sector_with_mapping
    ldr     r3, [r7]
    add     r5, r5, #1
    add     r3, r3, #1
    cmp     r6, r5
    str     r3, [r7]
    add     r4, r4, #0x200
    bne     .L3160
.L3184:
    add     sp, sp, #4
    pop     {r4, r5, r6, r7, r8, sl, lr}
    bx      lr
.L3190:
    .word   nrio_got - (.Lgotpc_30f0 + 16)
.L3194:
    .word   0x00000020
.L3198:
    .word   0x0000007c
.L319c:
    .word   0x00000024
.L31a0:
    .word   0x000000dc
.L31a4:
    .word   0x000000a4

.global nrio_init_nand_geometry
.type nrio_init_nand_geometry, %function
nrio_init_nand_geometry:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    ldr     sl, .L3468
    mov     r8, r1
    ldr     r3, .L346c
    ldr     r1, .L3470
    add     sl, pc, sl
    sub     sp, sp, #0x2c
    mov     r4, r0
    add     r1, sl, r1
    ldr     r0, [sl, r3]
    str     r2, [sp, #36]
    bl      nrio_require_word_alignment
    ldr     r3, .L3474
    ldr     r1, .L3478
    ldr     r2, .L347c
    ldr     r0, [sl, r3]
    add     r1, sl, r1
    str     r2, [sp, #4]
    bl      nrio_require_word_alignment
    ldr     r2, [sp, #4]
    ldr     r1, .L3480
    ldr     r3, [sl, r2]
    ldr     r2, .L3484
    ldr     r0, [r3]
    add     r1, sl, r1
    str     r2, [sp, #12]
    bl      nrio_require_word_alignment
    ldr     r2, [sp, #12]
    ldr     r1, .L3488
    ldr     r3, [sl, r2]
    add     r1, sl, r1
    ldr     r0, [r3]
    bl      nrio_require_word_alignment
    ldr     r3, .L348c
    ldr     r1, .L3490
    ldr     fp, [sl, r3]
    lsr     r4, r4, #11
    and     r4, r4, #0xff
    add     r1, sl, r1
    mov     r0, fp
    bl      nrio_require_word_alignment
    lsr     lr, r4, #6
    add     lr, lr, #5
    mov     r0, #0x3e
    lsl     r0, r0, lr
    ldr     r3, .L3494
    ldr     r2, .L3498
    str     r3, [sp]
    ldr     r3, .L349c
    str     r2, [sp, #20]
    str     r3, [sp, #16]
    ldr     r2, .L34a0
    ldr     r3, [sp]
    str     r2, [sp, #8]
    ldr     r9, [sl, r3]
    ldr     r2, [sp, #20]
    ldr     r3, [sp, #16]
    ldr     r5, [sl, r2]
    ldr     r6, [sl, r3]
    ldr     r2, [sp, #8]
    ldr     r3, .L34a4
    ldr     r7, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34a8
    sub     ip, r4, #1
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #36]
    str     r3, [r2]
    ldr     r3, .L34ac
    lsl     r1, r8, #16
    ldr     r2, [sl, r3]
    lsr     r1, r1, #22
    strh    r8, [r5]
    strh    r1, [r6]
    strb    lr, [r9]
    str     r0, [r7]
    strb    r4, [r2]
    mov     r0, fp
    mov     r2, #0
    add     r3, fp, #0x100
.L32ec:
    strb    r2, [r0], #1
    cmp     r0, r3
    bne     .L32ec
    ldr     r3, .L34b0
    ldr     r2, .L34b4
    ldr     r3, [sl, r3]
    ldr     r2, [sl, r2]
    str     r2, [r3]
    str     r3, [sp, #32]
    str     r2, [sp, #28]
    bl      nrio_find_block_table
    cmp     r0, #0
    bne     .L3324
.L3320:
    b       .L3320
.L3324:
    ldr     r3, .L34b8
    ldr     r4, [sl, r3]
    mvn     r2, #0xfe00
    strh    r2, [r4]
    bl      nrio_load_physical_block_table
    ldr     r2, [sp, #32]
    ldr     r3, [r2]
    ldr     r5, [r3, #252]
    ldr     r3, [sp]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #8]
    lsl     ip, r5, #16
    ldr     r0, [sl, r3]
    lsr     ip, ip, #22
    ldrb    r1, [r2]
    rsb     r3, ip, ip, lsl #5
    ldr     lr, [r0]
    lsl     r3, r3, #1
    add     r1, r1, #2
    rsb     lr, lr, r3, lsl r1
    ldr     r2, .L34bc
    ldr     r3, [sl, r2]
    ldr     r2, .L34c0
    ldrb    r0, [r3]
    ldr     r3, [sl, r2]
    ldrb    r6, [r3]
    ldr     r3, [sp, #16]
    ldr     r7, [sl, r3]
    ldr     r3, [sp, #12]
    ldr     r2, [sp, #20]
    ldr     r3, [sl, r3]
    ldr     r1, [sl, r2]
    str     r3, [sp, #24]
    ldr     r2, [sp, #4]
    ldr     r3, .L34c4
    ldr     r8, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34c8
    ldrb    fp, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34cc
    str     lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34d0
    strh    r5, [r1]
    mov     r1, #1
    strb    r1, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34d4
    str     r2, [r8]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #24]
    str     r2, [r3]
    ldr     r3, .L34d8
    ldr     r2, [sl, r3]
    ldr     r3, .L34dc
    strb    r0, [r2]
    ldr     r2, [sl, r3]
    strb    r6, [r2]
    ldr     r3, [sp, #28]
    ldr     r2, [sp, #32]
    str     r3, [r2]
    ldr     r3, .L34e0
    mov     r2, #0
    strh    r2, [r4]
    ldr     r2, [sl, r3]
    ldr     r3, .L34e4
    strb    fp, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L34e8
    ldrb    r1, [r2]
    ldr     r2, [sl, r3]
    strh    ip, [r7]
    strb    r1, [r2]
    bl      nrio_clear_block_map
    bl      nrio_clear_free_table
    bl      nrio_load_zone
    mov     r0, #0
    add     sp, sp, #0x2c
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L3468:
    .word   0x00001a44
.L346c:
    .word   0x000000ac
.L3470:
    .word   0xffffffa4
.L3474:
    .word   0x00000074
.L3478:
    .word   0xffffffb4
.L347c:
    .word   0x000000b8
.L3480:
    .word   0xffffffc0
.L3484:
    .word   0x00000054
.L3488:
    .word   0xffffffd0
.L348c:
    .word   0x0000006c
.L3490:
    .word   0xffffffe0
.L3494:
    .word   0x000000e0
.L3498:
    .word   0x0000000c
.L349c:
    .word   0x0000003c
.L34a0:
    .word   0x000000a0
.L34a4:
    .word   0x000000bc
.L34a8:
    .word   0x00000058
.L34ac:
    .word   0x00000068
.L34b0:
    .word   0x00000028
.L34b4:
    .word   0x00000088
.L34b8:
    .word   0x00000040
.L34bc:
    .word   0x00000090
.L34c0:
    .word   0x000000d0
.L34c4:
    .word   0x00000010
.L34c8:
    .word   0x00000020
.L34cc:
    .word   0x00000060
.L34d0:
    .word   0x000000cc
.L34d4:
    .word   0x000000d8
.L34d8:
    .word   0x000000e4
.L34dc:
    .word   0x00000044
.L34e0:
    .word   0x000000ec
.L34e4:
    .word   0x00000050
.L34e8:
    .word   0x0000004c
