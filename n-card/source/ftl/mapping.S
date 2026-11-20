@ Loads a mapping zone and translates logical sectors to physical pages.
.syntax unified
.arm
.section .nrio.2228, "ax", %progbits

.global nrio_load_zone
.type nrio_load_zone, %function
nrio_load_zone:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_222c:
    ldr     sl, .L2ec8
    ldr     r3, .L2ecc
    add     sl, pc, sl
    ldr     r9, [sl, r3]
    ldrb    r2, [r9]
    cmp     r2, #0
    sub     sp, sp, #0x5c
    bne     .L267c
    ldr     r3, .L2ed0
    ldr     r2, [sl, r3]
    ldrh    fp, [r2]
    cmp     fp, #0
    beq     .L2540
    ldr     ip, .L2ed4
    ldr     r3, [sl, ip]
    ldrh    r2, [r3]
    cmp     r2, fp
    str     ip, [sp, #12]
    beq     .L2bc0
    ldr     lr, .L2ed8
    ldr     r3, [sl, lr]
    ldrh    r2, [r3]
    cmp     r2, fp
    str     lr, [sp, #8]
    beq     .L28e8
    ldr     ip, .L2edc
    str     ip, [sp, #20]
    ldr     r3, [sl, ip]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L2a54
    ldr     r2, .L2ee0
    ldr     r3, .L2ee4
    ldr     lr, .L2ee8
    ldr     r0, .L2eec
    ldr     r1, .L2ef0
    str     r2, [sp, #24]
    str     r3, [sp, #28]
    str     lr, [sp, #16]
    str     r0, [sp, #4]
    str     r1, [sp]
    ldr     ip, [sp, #24]
    ldr     r3, [sl, lr]
    ldr     lr, [sp, #28]
    ldr     r2, [sl, r0]
    ldr     r0, [sl, ip]
    ldr     ip, [sl, lr]
    ldrb    lr, [r3]
    ldr     r3, .L2ef4
    ldr     r1, [sl, r1]
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2ef8
    ldrb    r5, [r1]
    strb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2efc
    ldrb    r6, [r0]
    strb    r5, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f00
    ldrb    r1, [ip]
    strb    r6, [r2]
    ldr     r2, [sl, r3]
    cmp     lr, #0
    strb    r1, [r2]
    bne     .L2ae0
.L2338:
    ldr     r3, .L2f04
    ldr     r2, .L2f08
    ldr     r6, [sl, r3]
    ldr     r3, .L2f0c
    str     r2, [sp, #32]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f10
    ldrb    r7, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f14
    ldrb    r8, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f18
    ldrb    r2, [r2]
    str     r2, [sp, #48]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #8]
    ldrb    r2, [r2]
    ldr     r3, [sl, r3]
    ldr     ip, [sp, #16]
    ldr     lr, [sp, #32]
    ldr     ip, [sl, ip]
    ldr     r1, [sp, #4]
    str     r2, [sp, #44]
    str     r3, [sp, #40]
    ldr     r2, [sp]
    ldr     r3, [sp, #24]
    ldr     r0, [sl, lr]
    ldr     r4, [sl, r3]
    ldr     lr, [sl, r2]
    str     ip, [sp, #36]
    ldr     r3, .L2f1c
    ldr     ip, [sl, r1]
    ldr     r2, .L2f20
    ldr     r1, [sp, #28]
    ldr     r5, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f24
    str     r1, [r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f28
    str     r3, [r0]
    ldr     r3, [sl, r2]
    str     r3, [r6]
    strb    r7, [ip]
    strb    r8, [lr]
    ldr     r2, [sp, #48]
    strb    r2, [r4]
    ldr     r3, [sp, #44]
    strb    r3, [r5]
    ldr     ip, [sp, #40]
    ldr     lr, [sp, #36]
    mov     r3, #1
    strh    fp, [ip]
    strb    r3, [lr]
.L2418:
    bl      nrio_clear_block_map
    ldr     r1, [sp, #4]
    ldr     r2, [sp]
    add     r3, sp, #24
    ldm     r3, {r3, lr}
    ldr     r0, [sl, r1]
    ldr     ip, [sl, r2]
    ldr     r1, [sl, lr]
    ldr     r2, [sl, r3]
    mov     r4, #0
    mvn     r3, #0
    strb    r3, [r2]
    strb    r3, [r0]
    strb    r3, [ip]
    strb    r4, [r1]
    bl      nrio_load_physical_block_table
    ldr     r3, .L2f04
    ldr     r6, [sl, r3]
    mov     r5, r4
.L2464:
    and     r0, r5, #0xff
    ldr     r4, [r6]
    bl      nrio_classify_physical_block
    strb    r0, [r5, r4]
    add     r5, r5, #1
    cmp     r5, #0x40
    bne     .L2464
    ldr     r0, [sp, #4]
    ldr     r3, [sl, r0]
    ldrb    r2, [r3]
    cmp     r2, #0xff
    beq     .L2524
    ldr     r1, [sp, #24]
    ldr     r3, [sp, #32]
    ldr     r2, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r5, .L2f2c
    ldrb    ip, [r2]
    ldr     r3, [r1]
    ldr     r0, [sl, r5]
    ldrb    r1, [ip, r3]
    ldr     ip, [sp]
    ldrb    lr, [r0]
    ldr     r2, [sl, ip]
    cmp     lr, #0
    strb    r1, [r2]
    beq     .L2524
    mov     r4, #0
.L24d4:
    ldr     lr, [sp, #4]
    ldr     r3, [sl, lr]
    mov     r1, r4
    ldrb    r0, [r3]
    bl      nrio_select_physical_page
    mov     r0, #10
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0
    ldreq   r0, [sp, #28]
    add     r3, r4, #1
    add     r1, r4, #1
    ldreq   r3, [sl, r0]
    andeq   r4, r1, #0xff
    andne   r4, r3, #0xff
    strbeq  r4, [r3]
    ldr     r3, [sl, r5]
    ldrb    r2, [r3]
    cmp     r2, r4
    bhi     .L24d4
.L2524:
    ldr     r1, [sp, #20]
    ldr     r2, [sl, r1]
    mov     r3, #1
    strb    r3, [r2]
.L2534:
    add     sp, sp, #0x5c
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L2540:
    ldr     r3, .L2edc
    ldr     r0, [sl, r3]
    ldrb    r2, [r0]
    cmp     r2, #0
    beq     .L2534
    ldr     r3, .L2ee8
    ldr     r2, [sl, r3]
    ldrb    r1, [r2]
    cmp     r1, #0
    strb    fp, [r0]
    beq     .L2870
    ldr     ip, .L2ef0
    ldr     lr, .L2ee0
    ldr     r3, .L2eec
    ldr     r0, .L2ee4
    str     r3, [sp, #4]
    str     r0, [sp, #28]
    str     ip, [sp]
    str     lr, [sp, #24]
    ldr     r3, [sl, r3]
    ldr     r2, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f0c
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f10
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f14
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r0, [sl, r0]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f18
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
.L25dc:
    ldr     r3, .L2ef4
    ldr     r2, [sl, r3]
    ldr     r3, .L2ef8
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2efc
    ldrb    r6, [r2]
    ldr     r2, [sp, #24]
    ldr     r7, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #28]
    ldr     lr, [sl, r3]
    ldr     r3, .L2f00
    ldr     r0, [sp, #4]
    ldrb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r1, [sp]
    ldr     r8, [sl, r0]
    ldr     r3, .L2f30
    ldrb    r0, [r2]
    ldr     r2, .L2f20
    ldr     r5, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f34
    str     r1, [r3]
    ldr     r3, .L2f08
    ldr     r1, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f38
    str     r1, [r2]
    ldr     r2, .L2f04
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    strb    r4, [r8]
    str     r1, [r3]
    strb    r6, [r5]
    strb    ip, [r7]
    strb    r0, [lr]
    b       .L2534
.L267c:
    ldr     r3, .L2f04
    ldr     r3, [sl, r3]
    ldr     r1, .L2eec
    str     r3, [sp, #84]
    ldr     r0, .L2f08
    ldr     r2, .L2ef0
    ldr     ip, .L2ee0
    ldr     lr, .L2ee4
    ldr     r3, .L2ef4
    str     r0, [sp, #32]
    str     r2, [sp]
    str     r1, [sp, #4]
    str     ip, [sp, #24]
    str     lr, [sp, #28]
    ldr     r2, [sl, r3]
    ldr     r3, .L2ef8
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2efc
    ldrb    fp, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f00
    ldrb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #28]
    ldrb    r2, [r2]
    ldr     r0, [sp]
    ldr     r6, [sl, r3]
    str     r2, [sp, #80]
    ldr     r3, .L2ee8
    ldr     r2, [sp, #32]
    ldr     r7, [sl, r1]
    ldr     r1, [sp, #24]
    ldr     r8, [sl, r0]
    ldr     r0, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r5, [sl, r1]
    mov     r1, #1
    strb    r1, [r2]
    ldr     r3, .L2f30
    ldr     r2, .L2f20
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f34
    str     r1, [r3]
    ldr     r3, [sl, r2]
    str     r3, [r0]
    ldr     r3, .L2edc
    ldr     r2, [sl, r3]
    ldr     r3, .L2ed4
    mov     r4, #0
    strb    r4, [r9]
    strb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2ed8
    strh    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f38
    ldr     r0, [sp, #84]
    strb    ip, [r5]
    strb    lr, [r7]
    strb    fp, [r8]
    ldr     r1, [sp, #80]
    strh    r4, [r2]
    ldr     r2, [sl, r3]
    strb    r1, [r6]
    str     r2, [r0]
    bl      nrio_clear_block_map
    mvn     r3, #0
    strb    r3, [r5]
    strb    r4, [r6]
    strb    r3, [r7]
    strb    r3, [r8]
    bl      nrio_load_physical_block_table
    ldr     r6, [sp, #84]
    mov     r5, r4
.L27ac:
    and     r0, r5, #0xff
    ldr     r4, [r6]
    bl      nrio_classify_physical_block
    strb    r0, [r5, r4]
    add     r5, r5, #1
    cmp     r5, #0x40
    bne     .L27ac
    ldr     r2, [sp, #4]
    ldr     r3, [sl, r2]
    ldrb    r2, [r3]
    cmp     r2, #0xff
    beq     .L2534
    ldr     r3, [sp, #24]
    ldr     ip, [sp, #32]
    ldr     r5, .L2f2c
    ldr     r2, [sl, r3]
    ldr     r1, [sl, ip]
    ldr     r0, [sl, r5]
    ldrb    ip, [r2]
    ldrb    lr, [r0]
    ldr     r3, [r1]
    ldr     r0, [sp]
    ldrb    r1, [ip, r3]
    ldr     r2, [sl, r0]
    cmp     lr, #0
    strb    r1, [r2]
    beq     .L2534
    mov     r4, #0
.L281c:
    ldr     r1, [sp, #4]
    ldr     r3, [sl, r1]
    mov     r1, r4
    ldrb    r0, [r3]
    bl      nrio_select_physical_page
    mov     r0, #10
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0
    ldreq   r2, [sp, #28]
    add     r3, r4, #1
    add     r1, r4, #1
    ldreq   r3, [sl, r2]
    andeq   r4, r1, #0xff
    andne   r4, r3, #0xff
    strbeq  r4, [r3]
    ldr     r3, [sl, r5]
    ldrb    r2, [r3]
    cmp     r2, r4
    bhi     .L281c
    b       .L2534
.L2870:
    ldr     r1, .L2eec
    ldr     ip, .L2ee4
    ldr     r2, .L2ef0
    ldr     r3, .L2ee0
    str     r1, [sp, #4]
    str     r3, [sp, #24]
    str     r2, [sp]
    str     ip, [sp, #28]
    ldr     r3, [sl, r1]
    ldr     lr, [sp, #24]
    ldr     r2, [sl, r2]
    ldr     r0, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f3c
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f40
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f44
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f48
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
    b       .L25dc
.L28e8:
    ldr     r0, .L2ee8
    str     r0, [sp, #16]
    ldr     r3, [sl, r0]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L2e38
    ldr     r1, .L2edc
    str     r1, [sp, #20]
    ldr     r3, [sl, r1]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L2534
.L2918:
    ldr     ip, .L2ef0
    ldr     lr, .L2ee0
    ldr     r3, .L2eec
    ldr     r0, .L2ee4
    str     r3, [sp, #4]
    str     r0, [sp, #28]
    str     ip, [sp]
    str     lr, [sp, #24]
    ldr     r3, [sl, r3]
    ldr     r2, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2ef4
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2ef8
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2efc
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r0, [sl, r0]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f00
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
.L2988:
    ldr     r0, [sp, #4]
    ldr     r1, [sp]
    ldr     r0, [sl, r0]
    ldr     r3, .L2f0c
    str     r0, [sp, #72]
    ldr     r2, [sl, r3]
    ldr     r1, [sl, r1]
    ldr     r3, .L2f10
    ldrb    fp, [r2]
    str     r1, [sp, #68]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f14
    ldrb    ip, [r2]
    ldr     r2, [sp, #24]
    ldr     lr, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #28]
    ldr     r5, [sl, r3]
    ldr     r3, .L2f18
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r1, [sp, #20]
    ldrb    r6, [r2]
    ldr     r3, .L2f1c
    ldr     r2, .L2f20
    ldr     r8, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f24
    str     r1, [r3]
    ldr     r3, .L2f08
    ldr     r1, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f28
    str     r1, [r2]
    ldr     r2, .L2f04
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, [sp, #72]
    ldr     r0, [sp, #16]
    strb    fp, [r2]
    ldr     r7, [sl, r0]
    str     r1, [r3]
    ldr     r3, [sp, #68]
    mov     r0, #1
    strb    ip, [r3]
    strb    r4, [lr]
    strb    r6, [r5]
    strb    r0, [r8]
    strb    r0, [r7]
    b       .L2534
.L2a54:
    ldr     r0, .L2ee8
    str     r0, [sp, #16]
    ldr     r3, [sl, r0]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L2dbc
    ldr     r1, .L2eec
    ldr     ip, .L2ee4
    ldr     r2, .L2ef0
    ldr     r3, .L2ee0
    str     r1, [sp, #4]
    str     r3, [sp, #24]
    str     r2, [sp]
    str     ip, [sp, #28]
    ldr     r3, [sl, r1]
    ldr     lr, [sp, #24]
    ldr     r2, [sl, r2]
    ldr     r0, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f0c
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f10
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f14
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f18
    ldrb    r5, [r0]
    strb    r4, [r2]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
.L2ae0:
    ldr     r3, .L2f04
    ldr     r0, .L2f08
    ldr     r6, [sl, r3]
    ldr     r3, .L2f3c
    str     r0, [sp, #32]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f40
    ldrb    r7, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f44
    ldrb    r8, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f48
    ldrb    r2, [r2]
    str     r2, [sp, #64]
    ldr     r2, [sl, r3]
    ldrb    r2, [r2]
    str     r2, [sp, #60]
    ldr     r2, [sp, #16]
    ldr     r3, [sp, #4]
    ldr     r2, [sl, r2]
    ldr     r1, [sp, #12]
    ldr     ip, [sl, r3]
    ldr     r1, [sl, r1]
    ldr     r3, [sp, #28]
    str     r2, [sp, #52]
    ldr     r2, [sp, #24]
    ldr     r5, [sl, r3]
    ldr     r4, [sl, r2]
    str     r1, [sp, #56]
    ldr     r3, .L2f4c
    ldr     r1, [sp]
    ldr     r2, .L2f20
    ldr     lr, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f50
    ldr     r0, [sl, r0]
    str     r1, [r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f54
    str     r3, [r0]
    ldr     r3, [sl, r2]
    str     r3, [r6]
    strb    r7, [ip]
    strb    r8, [lr]
    ldr     ip, [sp, #64]
    strb    ip, [r4]
    ldr     lr, [sp, #60]
    strb    lr, [r5]
    ldr     r0, [sp, #56]
    ldr     r1, [sp, #52]
    mov     r3, #0
    strh    fp, [r0]
    strb    r3, [r1]
    b       .L2418
.L2bc0:
    ldr     lr, .L2ee8
    str     lr, [sp, #16]
    ldr     r3, [sl, lr]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L2d2c
    ldr     r0, .L2edc
    str     r0, [sp, #20]
    ldr     r3, [sl, r0]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L2534
.L2bf0:
    ldr     r3, .L2ef0
    ldr     r2, .L2eec
    ldr     ip, .L2ee0
    ldr     lr, .L2ee4
    str     r3, [sp]
    str     r2, [sp, #4]
    str     ip, [sp, #24]
    str     lr, [sp, #28]
    ldr     r0, [sp]
    ldr     r3, [sl, r2]
    ldr     r1, [sl, ip]
    ldr     r2, [sl, r0]
    ldrb    ip, [r3]
    ldr     r3, .L2ef4
    ldr     r0, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2ef8
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2efc
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f00
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
.L2c64:
    ldr     r3, .L2f3c
    ldr     r2, [sl, r3]
    ldr     r3, .L2f40
    ldrb    ip, [r2]
    ldr     r2, [sp]
    ldr     lr, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #24]
    ldr     r5, [sl, r3]
    ldr     r3, .L2f44
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f48
    ldrb    r6, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, [sp, #20]
    ldr     r1, [sp, #4]
    ldrb    r8, [r2]
    ldr     r3, [sl, r3]
    ldr     r2, [sp, #16]
    str     r3, [sp, #76]
    ldr     r0, [sl, r1]
    ldr     fp, [sl, r2]
    ldr     r1, [sp, #28]
    ldr     r3, .L2f4c
    ldr     r2, .L2f20
    ldr     r7, [sl, r1]
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L2f50
    str     r1, [r3]
    ldr     r3, .L2f08
    ldr     r1, [sl, r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f54
    str     r1, [r2]
    ldr     r2, .L2f04
    ldr     r1, [sl, r3]
    ldr     r3, [sl, r2]
    str     r1, [r3]
    mov     r3, #0
    strb    ip, [r0]
    strb    r4, [lr]
    strb    r6, [r5]
    strb    r8, [r7]
    strb    r3, [fp]
    ldr     ip, [sp, #76]
    mov     r2, #1
    strb    r2, [ip]
    b       .L2534
.L2d2c:
    ldr     r1, .L2edc
    str     r1, [sp, #20]
    ldr     r3, [sl, r1]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L2bf0
    ldr     r1, .L2eec
    ldr     ip, .L2ee4
    ldr     r2, .L2ef0
    ldr     r3, .L2ee0
    str     r1, [sp, #4]
    str     r3, [sp, #24]
    str     r2, [sp]
    str     ip, [sp, #28]
    ldr     r3, [sl, r1]
    ldr     lr, [sp, #24]
    ldr     r2, [sl, r2]
    ldr     r0, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f0c
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f10
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f14
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f18
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
    b       .L2c64
.L2dbc:
    ldr     r2, .L2ee0
    ldr     r0, .L2eec
    ldr     r1, .L2ef0
    ldr     r3, .L2ee4
    str     r2, [sp, #24]
    str     r3, [sp, #28]
    str     r0, [sp, #4]
    str     r1, [sp]
    ldr     ip, [sp, #24]
    ldr     r3, [sl, r0]
    ldr     lr, [sp, #28]
    ldr     r2, [sl, r1]
    ldr     r1, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f3c
    ldr     r0, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f40
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f44
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f48
    ldrb    r5, [r0]
    strb    r4, [r2]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
    b       .L2338
.L2e38:
    ldr     r2, .L2edc
    str     r2, [sp, #20]
    ldr     r3, [sl, r2]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L2918
    ldr     r1, .L2eec
    ldr     ip, .L2ee4
    ldr     r2, .L2ef0
    ldr     r3, .L2ee0
    str     r1, [sp, #4]
    str     r3, [sp, #24]
    str     r2, [sp]
    str     ip, [sp, #28]
    ldr     r3, [sl, r1]
    ldr     lr, [sp, #24]
    ldr     r2, [sl, r2]
    ldr     r0, [sl, ip]
    ldrb    ip, [r3]
    ldr     r3, .L2f3c
    ldr     r1, [sl, lr]
    ldrb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f40
    strb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f44
    ldrb    r4, [r1]
    strb    lr, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L2f48
    strb    r4, [r2]
    ldrb    r5, [r0]
    ldr     r2, [sl, r3]
    strb    r5, [r2]
    b       .L2988
.L2ec8:
    .word   nrio_got - (.Lgotpc_222c + 16)
.L2ecc:
    .word   0x00000060
.L2ed0:
    .word   0x00000040
.L2ed4:
    .word   0x00000034
.L2ed8:
    .word   0x000000c0
.L2edc:
    .word   0x00000078
.L2ee0:
    .word   0x000000ec
.L2ee4:
    .word   0x0000004c
.L2ee8:
    .word   0x000000f0
.L2eec:
    .word   0x000000e4
.L2ef0:
    .word   0x00000044
.L2ef4:
    .word   0x00000090
.L2ef8:
    .word   0x000000d0
.L2efc:
    .word   0x00000010
.L2f00:
    .word   0x00000050
.L2f04:
    .word   0x00000054
.L2f08:
    .word   0x000000b8
.L2f0c:
    .word   0x000000c8
.L2f10:
    .word   0x00000098
.L2f14:
    .word   0x00000030
.L2f18:
    .word   0x0000008c
.L2f1c:
    .word   0x00000094
.L2f20:
    .word   0x00000028
.L2f24:
    .word   0x00000048
.L2f28:
    .word   0x000000a8
.L2f2c:
    .word   0x00000068
.L2f30:
    .word   0x00000088
.L2f34:
    .word   0x000000cc
.L2f38:
    .word   0x000000d8
.L2f3c:
    .word   0x000000f4
.L2f40:
    .word   0x0000005c
.L2f44:
    .word   0x00000038
.L2f48:
    .word   0x000000e8
.L2f4c:
    .word   0x00000070
.L2f50:
    .word   0x000000b0
.L2f54:
    .word   0x0000001c
