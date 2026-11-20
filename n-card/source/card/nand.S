@ Bulk Slot-1 data transfers and ECC generation. The command framing and
@ NAND opcodes are shared with bus.c.
.syntax unified
.arm

.section .nrio.0838, "ax", %progbits


.global nrio_finish_nand_write
.type nrio_finish_nand_write, %function
nrio_finish_nand_write:
    push    {r4, r5, r6, r7, sl, lr}
.Lgotpc_083c:
    ldr     sl, .L0900
    ldr     r3, .L0904
    add     sl, pc, sl
    ldr     r1, [sl, r3]
    ldr     r2, [r1]
    lsr     r2, r2, #18
    lsl     r2, r2, #18
    cmp     r2, #0
    mov     ip, r0
    beq     .L08f8
    mov     r0, #0x4000000
    add     r0, r0, #0x100
    mvn     r3, #0x2e
    mov     r1, #0x4000000
    strb    r3, [r0, #168]
    mov     r2, #0x44
    add     r3, r3, #0x2f
    mov     lr, ip
    strb    r2, [r1, #425]
    strb    r3, [r1, #430]
    mov     ip, r1
    mov     r7, r0
    add     r4, lr, #0x40
    mov     r6, #0xa7000000
    mov     r5, #0x4100000
.L08a0:
    ldr     r3, [lr], #4
    lsr     r2, r3, #8
    lsr     r1, r3, #16
    lsr     r0, r3, #24
    and     r2, r2, #0xff
    and     r1, r1, #0xff
    and     r3, r3, #0xff
    strb    r3, [ip, #426]
    strb    r2, [ip, #427]
    strb    r1, [r7, #172]
    strb    r0, [ip, #429]
    str     r6, [ip, #420]
.L08d0:
    ldr     r3, [ip, #420]
    tst     r3, #0x800000
    beq     .L08d0
    cmp     lr, r4
    ldr     r3, [r5, #16]
    bne     .L08a0
    mov     r0, #16
    bl      nrio_send_nand_command
    pop     {r4, r5, r6, r7, sl, lr}
    b       nrio_wait_nand
.L08f8:
    pop     {r4, r5, r6, r7, sl, lr}
    bx      lr
.L0900:
    .word   nrio_got - (.Lgotpc_083c + 16)
.L0904:
    .word   0x000000c4

.section .nrio.0a5c, "ax", %progbits

.global nrio_read_nand_sector_with_ecc
.type nrio_read_nand_sector_with_ecc, %function
nrio_read_nand_sector_with_ecc:
    push    {r4, r5, r6, r7, r8, r9, fp, lr}
    mov     r4, r1
    sub     sp, sp, #24
    mov     r5, r0
    mov     r0, #5
    str     r2, [sp, #4]
    bl      nrio_send_nand_command
    lsl     r0, r4, #9
    bl      nrio_command_52
    mov     r0, #0xe0
    bl      nrio_send_nand_command
    mov     r2, #0x4000000
    mov     r1, #0
    add     r2, r2, #0x100
    mov     r0, #0x4000000
    mvn     r3, #0x2f
    strb    r3, [r2, #168]
    str     r1, [sp, #8]
    strb    r1, [r0, #425]
    ldr     r9, [sp, #8]
    mov     r3, #0xa1000000
    and     r2, r5, #3
    str     r3, [r0, #420]
    str     r1, [sp, #12]
    str     r9, [sp, #16]
    str     r9, [sp, #20]
    add     r5, r5, r2
    mov     r7, r9
    mov     fp, r9
    mov     r6, #0x4100000
.L0ad4:
    mov     r1, #0x4000000
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0ad4
    ldr     r0, [r6, #16]
    str     r0, [r5], #4
.L0aec:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0aec
    ldr     r4, [r6, #16]
    str     r4, [r5], #4
    eor     ip, r0, r4
    mov     r1, #0x4000000
.L0b08:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0b08
    ldr     r0, [r6, #16]
    str     r0, [r5], #4
    eor     lr, ip, r0
    mov     r1, #0x4000000
.L0b24:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0b24
    ldr     ip, [r6, #16]
    eor     r3, r7, r0
    str     ip, [r5], #4
    eor     r0, lr, ip
    eor     r8, r3, ip
    mov     r1, #0x4000000
.L0b48:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0b48
    ldr     r2, [r6, #16]
    str     r2, [r5], #4
    eor     r0, r0, r2
    mov     r1, #0x4000000
.L0b64:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0b64
    ldr     r1, [sp, #16]
    ldr     lr, [r6, #16]
    eor     r3, r1, r4
    eor     r3, r3, ip
    str     lr, [r5], #4
    eor     r0, r0, lr
    eor     r7, r3, lr
    mov     r1, #0x4000000
.L0b90:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0b90
    ldr     ip, [r6, #16]
    str     ip, [r5], #4
    eor     r0, r0, ip
    eor     r4, r8, ip
    mov     r1, #0x4000000
.L0bb0:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0bb0
    ldr     r1, [r6, #16]
    eor     r0, r0, r1
    eor     r3, r0, r0, lsr #16
    ldr     r8, [sp, #12]
    eor     r3, r3, r3, lsr #8
    eor     r3, r3, r3, lsr #4
    eor     r3, r3, r3, lsr #2
    eor     r2, r8, r2
    and     r3, r3, #3
    eor     r2, r2, lr
    sub     r3, r3, #1
    eor     r2, r2, ip
    eor     r7, r7, r1
    eor     r2, r2, r1
    cmp     r3, #1
    str     r7, [sp, #16]
    str     r2, [sp, #12]
    str     r1, [r5]
    eor     r7, r4, r1
    eor     fp, fp, r0
    bls     .L0c24
    add     r9, r9, #1
    cmp     r9, #15
    bhi     .L0c48
.L0c1c:
    add     r5, r5, #4
    b       .L0ad4
.L0c24:
    ldr     ip, [sp, #8]
    ldr     r0, [sp, #20]
    eor     ip, ip, r9
    add     r9, r9, #1
    eor     r0, r0, #1
    cmp     r9, #15
    str     ip, [sp, #8]
    str     r0, [sp, #20]
    bls     .L0c1c
.L0c48:
    add     r2, sp, #12
    ldm     r2, {r2, r5}
    bic     r4, fp, #0xf000000
    eor     r1, r2, r2, lsr #16
    eor     ip, r7, r7, lsr #16
    bic     r4, r4, #0xf0000
    bic     r0, fp, #0x55000000
    eor     lr, r5, r5, lsr #16
    eor     r1, r1, r1, lsl #8
    eor     ip, ip, ip, lsr #8
    bic     r4, r4, #0xf00
    bic     r0, r0, #0x550000
    bic     r3, fp, #0x33000000
    eor     r1, r1, r1, lsr #4
    eor     ip, ip, ip, lsl #4
    eor     lr, lr, lr, lsr #8
    bic     r4, r4, #15
    bic     r0, r0, #0x5500
    bic     r3, r3, #0x330000
    bic     r6, fp, #0xff0000
    eor     r1, r1, r1, lsr #2
    eor     ip, ip, ip, lsl #2
    eor     lr, lr, lr, lsl #4
    eor     r4, r4, r4, lsl #16
    bic     r0, r0, #0x55
    bic     r6, r6, #0xff
    bic     r3, r3, #0x3300
    eor     r1, r1, r1, lsl #1
    eor     ip, ip, ip, lsl #1
    eor     lr, lr, lr, lsr #2
    eor     r4, r4, r4, lsr #8
    eor     r0, r0, r0, lsl #16
    eor     r6, r6, r6, lsr #16
    and     ip, ip, #0x80
    bic     r3, r3, #0x33
    lsr     r2, fp, #24
    and     r1, r1, #0x200
    eor     lr, lr, lr, lsl #1
    eor     r4, r4, r4, lsl #2
    eor     r0, r0, r0, lsr #8
    orr     r1, r1, ip
    eor     r2, r2, fp, lsr #16
    eor     r3, r3, r3, lsl #16
    lsr     r5, r6, #12
    and     lr, lr, #32
    eor     r4, r4, r4, lsl #1
    eor     r0, r0, r0, lsr #4
    ldr     r8, [sp, #8]
    eor     r5, r5, r6, lsr #8
    eor     r2, r2, r2, lsr #4
    eor     r3, r3, r3, lsr #8
    orr     r1, r1, lr
    and     r4, r4, #0x800000
    eor     r0, r0, r0, lsl #2
    and     r0, r0, #0x80000
    eor     r2, r2, r2, lsl #2
    eor     r5, r5, r5, lsr #2
    eor     r3, r3, r3, lsl #4
    orr     r1, r1, r4
    orr     r1, r1, r0
    eor     r2, r2, r2, lsl #1
    and     r0, r8, #2
    eor     r5, r5, r5, lsl #1
    lsr     r3, r3, #2
    orr     r1, r1, r0, lsl #12
    and     ip, r8, #8
    and     r2, r2, #8
    and     r5, r5, #2
    eor     r3, r3, r3, lsl #1
    orr     r1, r1, ip, lsl #14
    and     r3, r3, #0x200000
    ldr     ip, [sp, #20]
    orr     r2, r2, r5
    orr     r2, r2, r3
    and     r3, r8, #1
    cmp     ip, #0
    orr     r2, r2, r3, lsl #11
    and     r3, r8, #4
    orr     r2, r2, r3, lsl #13
    movne   r3, #0xaa0000
    addne   r3, r3, #0xaa00
    orr     r0, r2, r1
    addne   r3, r3, #0xaa
    eorne   r3, r0, r3
    orrne   r0, r0, r3, lsr #1
    orreq   r0, r0, r0, lsr #1
    mvn     r3, r0
    ldr     r0, [sp, #4]
    lsr     r2, r3, #16
    lsr     r1, r3, #8
    strb    r3, [r0]
    strb    r2, [r0, #2]
    strb    r1, [r0, #1]
    add     sp, sp, #24
    pop     {r4, r5, r6, r7, r8, r9, fp, lr}
    bx      lr

.global nrio_write_nand_sector_with_ecc
.type nrio_write_nand_sector_with_ecc, %function
nrio_write_nand_sector_with_ecc:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_0dcc:
    ldr     sl, .L132c
    ldr     r3, .L1330
    add     sl, pc, sl
    ldr     lr, [sl, r3]
    ldr     ip, [lr]
    lsr     ip, ip, #18
    lsl     ip, ip, #18
    sub     sp, sp, #0x2c
    cmp     ip, #0
    mov     r5, r0
    mov     r4, r1
    str     r2, [sp, #4]
    beq     .L130c
    cmp     r1, #0
    beq     .L1318
.L0e08:
    mov     r2, #0x4000000
    mov     r0, #0x4000000
    mov     lr, #0
    add     r2, r2, #0x100
    mvn     r3, #0x2c
    strb    r3, [r2, #168]
    strb    lr, [r0, #425]
    mov     r2, r5
    ldrb    r1, [r2], #1
    mov     r3, #0xe1000000
    add     r3, r3, #8
    strb    r1, [r0, #431]
    str     r3, [r0, #420]
    ldrb    r3, [r2, #1]
    ldrb    r1, [r5, #1]
    add     r2, r2, #1
    ldrb    r0, [r2, #1]
    add     r2, r2, #1
    ldrb    ip, [r2, #1]
    orr     r1, r1, r3, lsl #8
    str     lr, [sp, #12]
    add     lr, r2, #2
    mov     r2, #0
    orr     r1, r1, r0, lsl #16
    and     r3, r5, #3
    str     r2, [sp, #8]
    str     r2, [sp, #16]
    str     r2, [sp, #20]
    str     r2, [sp, #24]
    str     r2, [sp, #28]
    str     r2, [sp, #32]
    orr     r1, r1, ip, lsl #24
    add     r0, r5, r3
    mov     r9, #0x4100000
.L0e90:
    mov     r6, #0x4000000
    ldr     r3, [r6, #420]
    tst     r3, #0x800000
    beq     .L0e90
    str     r1, [r9, #16]
    mov     r3, lr
    ldrb    r7, [r3], #1
    ldr     r8, [r0], #4
    add     r2, r3, #1
    ldrb    r4, [r3, #1]
    ldrb    r5, [lr, #1]
    add     r1, r2, #2
    ldrb    lr, [r2, #1]
    mov     ip, r0
    mov     r2, r6
.L0ecc:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L0ecc
    orr     r3, r7, r5, lsl #8
    orr     r3, r3, r4, lsl #16
    orr     r3, r3, lr, lsl #24
    str     r3, [r9, #16]
    mov     r2, r1
    ldrb    r3, [r2], #1
    ldrb    r1, [r1, #1]
    ldrb    r0, [r2, #1]
    add     r2, r2, #1
    orr     r3, r3, r1, lsl #8
    ldr     fp, [ip], #4
    ldrb    r1, [r2, #1]
    orr     r3, r3, r0, lsl #16
    orr     r0, r3, r1, lsl #24
    mov     lr, ip
    add     r1, r2, #2
    eor     r4, r8, fp
    mov     r2, #0x4000000
.L0f20:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L0f20
    str     r0, [r9, #16]
    mov     r3, r1
    ldrb    r2, [r3], #1
    ldrb    r1, [r1, #1]
    ldrb    r0, [r3, #1]
    add     r3, r3, #1
    orr     r2, r2, r1, lsl #8
    ldr     ip, [lr], #4
    ldrb    r1, [r3, #1]
    orr     r2, r2, r0, lsl #16
    orr     r2, r2, r1, lsl #24
    add     r0, r3, #2
    eor     r5, r4, ip
    mov     r1, #0x4000000
.L0f64:
    ldr     r3, [r1, #420]
    tst     r3, #0x800000
    beq     .L0f64
    str     r2, [r9, #16]
    mov     r3, r0
    ldrb    r2, [r3], #1
    ldrb    r1, [r0, #1]
    ldrb    r0, [r3, #1]
    orr     r2, r2, r1, lsl #8
    orr     r2, r2, r0, lsl #16
    add     r3, r3, #1
    ldr     r0, [sp, #20]
    ldr     r4, [lr], #4
    ldrb    r1, [r3, #1]
    eor     ip, ip, r0
    eor     r5, r5, r4
    orr     r0, r2, r1, lsl #24
    eor     r8, ip, r4
    add     r1, r3, #2
    mov     r2, #0x4000000
.L0fb4:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L0fb4
    str     r0, [r9, #16]
    mov     r3, r1
    ldrb    r2, [r3], #1
    ldrb    r1, [r1, #1]
    ldrb    r0, [r3, #1]
    add     r3, r3, #1
    orr     r2, r2, r1, lsl #8
    ldr     r7, [lr], #4
    ldrb    r1, [r3, #1]
    orr     r2, r2, r0, lsl #16
    orr     r0, r2, r1, lsl #24
    eor     r6, r5, r7
    add     r1, r3, #2
    mov     r2, #0x4000000
.L0ff8:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L0ff8
    str     r0, [r9, #16]
    mov     r3, r1
    ldr     r5, [sp, #24]
    ldrb    r2, [r3], #1
    ldrb    r0, [r1, #1]
    eor     r1, fp, r5
    ldr     r5, [lr], #4
    ldrb    ip, [r3, #1]
    eor     r1, r1, r4
    add     r3, r3, #1
    orr     r2, r2, r0, lsl #8
    eor     r1, r1, r5
    ldrb    r0, [r3, #1]
    orr     r2, r2, ip, lsl #16
    str     r1, [sp, #36]
    orr     r4, r2, r0, lsl #24
    eor     ip, r6, r5
    add     r0, r3, #2
    mov     r2, #0x4000000
.L1050:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L1050
    str     r4, [r9, #16]
    mov     r3, r0
    ldrb    r2, [r3], #1
    ldrb    r1, [r0, #1]
    ldrb    r0, [r3, #1]
    add     r3, r3, #1
    ldr     r4, [lr], #4
    orr     r2, r2, r1, lsl #8
    ldrb    r1, [r3, #1]
    orr     r2, r2, r0, lsl #16
    orr     r1, r2, r1, lsl #24
    add     r6, r3, #2
    mov     fp, lr
    eor     ip, ip, r4
    eor     r8, r8, r4
    mov     r2, #0x4000000
.L109c:
    ldr     r3, [r2, #420]
    tst     r3, #0x800000
    beq     .L109c
    str     r1, [r9, #16]
    ldr     r0, [fp]
    ldr     lr, [sp, #16]
    eor     ip, ip, r0
    eor     r2, r7, lr
    eor     r3, ip, ip, lsr #16
    eor     r2, r2, r5
    eor     r3, r3, r3, lsr #8
    eor     r2, r2, r4
    eor     r3, r3, r3, lsr #4
    mov     r1, r6
    eor     r2, r2, r0
    eor     r3, r3, r3, lsr #2
    ldrb    r7, [r1], #1
    ldr     r5, [sp, #28]
    str     r2, [sp, #16]
    and     r3, r3, #3
    ldr     r2, [sp, #36]
    sub     r3, r3, #1
    add     lr, r1, #1
    eor     r2, r2, r0
    cmp     r3, #1
    eor     r8, r8, r0
    eor     r5, r5, ip
    ldrb    r4, [r1, #1]
    ldrb    r3, [r6, #1]
    ldrb    r1, [lr, #1]
    str     r2, [sp, #24]
    str     r8, [sp, #20]
    str     r5, [sp, #28]
    add     r2, lr, #1
    bls     .L1154
    ldr     r5, [sp, #8]
    add     r5, r5, #1
    cmp     r5, #15
    str     r5, [sp, #8]
    bhi     .L1184
.L113c:
    orr     r3, r7, r3, lsl #8
    orr     r3, r3, r4, lsl #16
    orr     r1, r3, r1, lsl #24
    add     lr, r2, #1
    add     r0, fp, #4
    b       .L0e90
.L1154:
    add     r5, sp, #8
    ldm     r5, {r5, ip}
    ldr     lr, [sp, #8]
    ldr     r0, [sp, #32]
    add     r5, r5, #1
    eor     ip, ip, lr
    eor     r0, r0, #1
    cmp     r5, #15
    str     ip, [sp, #12]
    str     r0, [sp, #32]
    str     r5, [sp, #8]
    bls     .L113c
.L1184:
    ldr     ip, [sp, #28]
    ldr     r2, [sp, #28]
    ldr     lr, [sp, #16]
    add     r0, sp, #20
    ldm     r0, {r0, r5}
    bic     r4, ip, #0xf000000
    bic     r3, ip, #0x33000000
    eor     r1, lr, lr, lsr #16
    eor     ip, r0, r0, lsr #16
    bic     r4, r4, #0xf0000
    bic     r0, r2, #0x55000000
    eor     lr, r5, r5, lsr #16
    eor     r1, r1, r1, lsl #8
    eor     ip, ip, ip, lsr #8
    bic     r4, r4, #0xf00
    bic     r0, r0, #0x550000
    bic     r6, r2, #0xff0000
    bic     r3, r3, #0x330000
    eor     r1, r1, r1, lsr #4
    eor     ip, ip, ip, lsl #4
    eor     lr, lr, lr, lsr #8
    bic     r4, r4, #15
    bic     r0, r0, #0x5500
    ldr     r5, [sp, #28]
    bic     r6, r6, #0xff
    bic     r3, r3, #0x3300
    eor     r1, r1, r1, lsr #2
    eor     ip, ip, ip, lsl #2
    eor     lr, lr, lr, lsl #4
    eor     r4, r4, r4, lsl #16
    bic     r0, r0, #0x55
    eor     r6, r6, r6, lsr #16
    bic     r3, r3, #0x33
    lsr     r2, r2, #24
    eor     r1, r1, r1, lsl #1
    eor     ip, ip, ip, lsl #1
    eor     lr, lr, lr, lsr #2
    eor     r4, r4, r4, lsr #8
    eor     r0, r0, r0, lsl #16
    eor     r2, r2, r5, lsr #16
    and     ip, ip, #0x80
    eor     r3, r3, r3, lsl #16
    lsr     r5, r6, #12
    and     r1, r1, #0x200
    eor     lr, lr, lr, lsl #1
    eor     r4, r4, r4, lsl #2
    eor     r0, r0, r0, lsr #8
    orr     r1, r1, ip
    and     lr, lr, #32
    eor     r5, r5, r6, lsr #8
    eor     r2, r2, r2, lsr #4
    eor     r3, r3, r3, lsr #8
    eor     r4, r4, r4, lsl #1
    eor     r0, r0, r0, lsr #4
    ldr     ip, [sp, #12]
    orr     r1, r1, lr
    eor     r2, r2, r2, lsl #2
    eor     r5, r5, r5, lsr #2
    eor     r3, r3, r3, lsl #4
    and     r4, r4, #0x800000
    eor     r0, r0, r0, lsl #2
    and     r0, r0, #0x80000
    eor     r2, r2, r2, lsl #1
    eor     r5, r5, r5, lsl #1
    lsr     r3, r3, #2
    orr     r1, r1, r4
    and     r5, r5, #2
    orr     r1, r1, r0
    and     r2, r2, #8
    and     r0, ip, #2
    eor     r3, r3, r3, lsl #1
    ldr     lr, [sp, #12]
    orr     r2, r2, r5
    and     r3, r3, #0x200000
    orr     r1, r1, r0, lsl #12
    ldr     r0, [sp, #32]
    orr     r2, r2, r3
    and     r3, ip, #1
    cmp     r0, #0
    orr     r2, r2, r3, lsl #11
    and     r3, lr, #4
    orr     r2, r2, r3, lsl #13
    and     ip, ip, #8
    movne   r3, #0xaa0000
    addne   r3, r3, #0xaa00
    orr     r1, r1, ip, lsl #14
    orr     r0, r2, r1
    addne   r3, r3, #0xaa
    eorne   r3, r0, r3
    orrne   r0, r0, r3, lsr #1
    orreq   r0, r0, r0, lsr #1
    mvn     r3, r0
    ldr     r5, [sp, #4]
    lsr     r2, r3, #16
    lsr     r1, r3, #8
    strb    r3, [r5]
    strb    r2, [r5, #2]
    strb    r1, [r5, #1]
.L130c:
    add     sp, sp, #0x2c
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L1318:
    mov     r0, #0x80
    bl      nrio_send_nand_command
    mov     r0, r4
    bl      nrio_command_55
    b       .L0e08
.L132c:
    .word   nrio_got - (.Lgotpc_0dcc + 16)
.L1330:
    .word   0x000000c4
