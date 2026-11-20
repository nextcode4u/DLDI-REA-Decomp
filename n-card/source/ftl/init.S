@ Reads the NAND identification and geometry, then mounts the mapping tables.
.syntax unified
.arm
.section .nrio.34ec, "ax", %progbits

.global nrio_ftl_init
.type nrio_ftl_init, %function
nrio_ftl_init:
    push    {r4, lr}
    sub     sp, sp, #8
    mov     r4, r0
    add     r0, sp, #4
    bl      nrio_read_nand_id
    ldrb    r3, [sp, #4]
    cmp     r3, #0xec
    cmpne   r3, #0xad
    beq     .L3524
    cmp     r3, #0x2c
    beq     .L3524
    cmp     r3, #0x98
    beq     .L3524
.L3520:
    b       .L3520
.L3524:
    ldrb    r3, [sp, #5]
    sub     r3, r3, #0x71
    cmp     r3, #0x80
    addls   pc, pc, r3, lsl #2
    b       .L37b0
    b       .L378c
    b       .L37b0
    b       .L37b0
    b       .L37d8
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37d8
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37d8
    b       .L37b0
    b       .L373c
    b       .L37b0
    b       .L3768
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L378c
    b       .L37b0
    b       .L37b4
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L373c
    b       .L37b0
    b       .L3768
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L378c
    b       .L37b0
    b       .L37b4
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37b0
    b       .L37d8
.L373c:
    ldrb    r3, [sp, #7]
    tst     r3, #32
    beq     .L37fc
    mov     r3, r4
    mov     r1, #0xfc0
    mov     r2, #0x1000000
    mov     r0, #0x40000
    bl      nrio_init_nand_geometry
.L375c:
    add     sp, sp, #8
    pop     {r4, lr}
    bx      lr
.L3768:
    ldrb    r3, [sp, #7]
    tst     r3, #32
    beq     .L3844
    mov     r3, r4
    mov     r1, #0x1fc0
    mov     r2, #0x1000000
    mov     r0, #0x40000
    bl      nrio_init_nand_geometry
    b       .L375c
.L378c:
    ldrb    r3, [sp, #7]
    tst     r3, #32
    beq     .L3814
    mov     r3, r4
    mov     r1, #0x3c0
    mov     r2, #0x1000000
    mov     r0, #0x40000
    bl      nrio_init_nand_geometry
    b       .L375c
.L37b0:
    b       .L37b0
.L37b4:
    ldrb    r3, [sp, #7]
    tst     r3, #32
    beq     .L382c
    mov     r3, r4
    mov     r1, #0x7c0
    mov     r2, #0x1000000
    mov     r0, #0x40000
    bl      nrio_init_nand_geometry
    b       .L375c
.L37d8:
    ldrb    r3, [sp, #7]
    tst     r3, #32
    beq     .L385c
    mov     r3, r4
    mov     r1, #0x1c0
    mov     r2, #0x1000000
    mov     r0, #0x40000
    bl      nrio_init_nand_geometry
    b       .L375c
.L37fc:
    mov     r3, r4
    mov     r1, #0x1fc0
    mov     r2, #0x800000
    mov     r0, #0x20000
    bl      nrio_init_nand_geometry
    b       .L375c
.L3814:
    mov     r3, r4
    mov     r1, #0x7c0
    mov     r2, #0x800000
    mov     r0, #0x20000
    bl      nrio_init_nand_geometry
    b       .L375c
.L382c:
    mov     r3, r4
    mov     r1, #0xfc0
    mov     r2, #0x800000
    mov     r0, #0x20000
    bl      nrio_init_nand_geometry
    b       .L375c
.L3844:
    mov     r3, r4
    mov     r1, #0x3fc0
    mov     r2, #0x800000
    mov     r0, #0x20000
    bl      nrio_init_nand_geometry
    b       .L375c
.L385c:
    mov     r3, r4
    mov     r1, #0x3c0
    mov     r2, #0x800000
    mov     r0, #0x20000
    bl      nrio_init_nand_geometry
    b       .L375c
