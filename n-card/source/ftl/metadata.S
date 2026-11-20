@ Tracks the sector cursor and replaces a pending block after a copy failure.
.syntax unified
.arm
.section .nrio.3874, "ax", %progbits

.global nrio_read_sector_with_ecc_mapping
.type nrio_read_sector_with_ecc_mapping, %function
nrio_read_sector_with_ecc_mapping:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_3878:
    ldr     sl, .L3b68
    ldr     r9, .L3b6c
    add     sl, pc, sl
    ldr     r3, [sl, r9]
    ldrb    r2, [r3]
    cmp     r2, #0
    sub     sp, sp, #20
    beq     .L3a38
    ldr     r3, .L3b70
    ldr     r2, [sl, r3]
    ldr     r3, .L3b74
    ldr     ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L3b78
    ldr     r1, [r2]
    ldr     r2, [sl, r3]
    add     ip, ip, r1
    ldrb    lr, [r2]
    lsr     r4, ip, #2
    lsr     lr, r4, lr
    mov     r3, #0x84000003
    add     r3, r3, #0x210000
    add     r3, r3, #0x840
    lsr     r2, lr, #1
    umull   r1, r0, r3, r2
    ldr     r2, .L3b7c
    ldr     r3, .L3b80
    str     r2, [sp, #8]
    str     r3, [sp, #12]
    ldr     r3, .L3b84
    ldr     r5, [sp, #8]
    ldr     r1, .L3b88
    lsr     r0, r0, #4
    ldr     r2, [sl, r3]
    lsl     r3, r0, #16
    lsr     r7, r3, #16
    ldr     r8, [sl, r1]
    ldr     r3, [sp, #12]
    ldr     r5, [sl, r5]
    ldr     fp, .L3b8c
    ldrb    r6, [r2]
    str     r5, [sp, #4]
    ldrh    r2, [r8]
    ldr     r5, [sl, r3]
    ldr     r3, .L3b90
    ldr     r1, [sl, fp]
    cmp     r2, r7
    and     ip, ip, #3
    ldr     r2, [sl, r3]
    ldr     r3, .L3b94
    strb    ip, [r1]
    ldr     r1, [sp, #4]
    rsb     r0, r0, r0, lsl #5
    str     lr, [r2]
    ldr     r2, [sl, r3]
    sub     r0, lr, r0, lsl #1
    and     r4, r4, r6
    strb    r4, [r1]
    strb    r0, [r5]
    strh    r7, [r2]
    strhne  r7, [r8]
    blne    nrio_load_zone
.L3970:
    ldr     r3, [sl, fp]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L3a88
.L3980:
    ldr     r1, [sp, #12]
    ldr     r2, .L3b98
    ldr     r3, [sl, r1]
    ldrb    ip, [r3]
    ldr     r3, [sl, r2]
    ldr     r2, .L3b9c
    ldr     r0, [r3]
    ldr     r3, [sl, r2]
    ldrb    r1, [r3]
    ldr     r3, .L3ba0
    ldrb    r0, [ip, r0]
    ldr     r2, [sl, r3]
    cmp     r1, ip
    strb    r0, [r2]
    beq     .L3a9c
    cmp     r0, #0xff
    beq     .L3b40
    ldr     r5, [sp, #8]
    ldr     r3, [sl, r5]
    ldrb    r1, [r3]
    bl      nrio_select_physical_page
.L39d4:
    mov     r0, #1
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
.L39e0:
    ldr     ip, [sl, fp]
    ldr     r3, .L3ba4
    ldrb    r5, [ip]
    ldr     r1, [sl, r3]
    add     r3, r5, #1
    ldr     r2, [r1]
    and     r3, r3, #3
    strb    r3, [ip]
    ldr     r3, .L3ba8
    sub     r2, r2, #1
    ldr     lr, [sl, r9]
    str     r2, [r1]
    ldr     r2, [sl, r3]
    mov     r4, #0
    mov     r0, #2
    strb    r4, [lr]
    strb    r5, [r2]
    bl      nrio_page_operation
    mov     r0, r4
    add     sp, sp, #20
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L3a38:
    ldr     fp, .L3b8c
    ldr     r3, [sl, fp]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L39e0
    ldr     r2, .L3b7c
    str     r2, [sp, #8]
    ldr     ip, [sl, r2]
    ldr     r2, .L3b84
    ldr     r1, [sl, r2]
    ldrb    r3, [ip]
    ldrb    r0, [r1]
    add     r3, r3, #1
    and     r3, r3, r0
    cmp     r3, #0
    strb    r3, [ip]
    beq     .L3adc
    ldr     lr, .L3b80
    str     lr, [sp, #12]
    b       .L3980
.L3a88:
    ldr     r3, [sl, r9]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L3980
    b       .L39e0
.L3a9c:
    ldr     r2, [sp, #8]
    ldr     r3, [sl, r2]
    ldr     r2, .L3bac
    ldrb    ip, [r3]
    ldr     r3, [sl, r2]
    ldrb    r1, [r3]
    cmp     r1, ip
    bhi     .L3b50
    ldr     r3, .L3bb0
    ldr     r2, [sl, r3]
    ldrb    r0, [r2]
    cmp     r0, #0xff
    beq     .L3b40
    mov     r1, ip
    bl      nrio_select_physical_page
    b       .L39d4
.L3adc:
    ldr     r3, .L3b80
    str     r3, [sp, #12]
    ldr     ip, [sl, r3]
    ldrb    r2, [ip]
    mov     r1, #0x84000003
    add     r2, r2, #1
    and     r2, r2, #0xff
    add     r1, r1, #0x210000
    lsr     r0, r2, #1
    add     r1, r1, #0x840
    umull   r5, r3, r1, r0
    lsr     r3, r3, #4
    rsb     r3, r3, r3, lsl #5
    sub     r2, r2, r3, lsl #1
    and     r2, r2, #0xff
    cmp     r2, #0
    strb    r2, [ip]
    bne     .L3980
    ldr     r3, .L3b88
    ldr     r1, [sl, r3]
    ldrh    r2, [r1]
    add     r2, r2, #1
    strh    r2, [r1]
    bl      nrio_load_zone
    b       .L3970
.L3b40:
    bl      nrio_find_free_block
    mov     r1, #0
    bl      nrio_select_physical_page
    b       .L39d4
.L3b50:
    ldr     r3, .L3bb4
    ldr     r2, [sl, r3]
    mov     r1, ip
    ldrb    r0, [r2]
    bl      nrio_select_physical_page
    b       .L39d4
.L3b68:
    .word   nrio_got - (.Lgotpc_3878 + 16)
.L3b6c:
    .word   0x00000024
.L3b70:
    .word   0x0000007c
.L3b74:
    .word   0x000000a0
.L3b78:
    .word   0x000000e0
.L3b7c:
    .word   0x000000b4
.L3b80:
    .word   0x0000009c
.L3b84:
    .word   0x000000bc
.L3b88:
    .word   0x00000040
.L3b8c:
    .word   0x00000080
.L3b90:
    .word   0x0000002c
.L3b94:
    .word   0x00000084
.L3b98:
    .word   0x000000b8
.L3b9c:
    .word   0x000000ec
.L3ba0:
    .word   0x00000014
.L3ba4:
    .word   0x000000dc
.L3ba8:
    .word   0x000000d4
.L3bac:
    .word   0x0000004c
.L3bb0:
    .word   0x00000044
.L3bb4:
    .word   0x000000e4

.section .nrio.3c30, "ax", %progbits
