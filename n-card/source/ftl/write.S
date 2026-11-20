@ Sector writes, block replacement, and handling of partially written pages.
.syntax unified
.arm
.section .nrio.3fc0, "ax", %progbits

.global nrio_write_sector_with_mapping
.type nrio_write_sector_with_mapping, %function
nrio_write_sector_with_mapping:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_3fc4:
    ldr     sl, .L4920
    ldr     r1, .L4924
    add     sl, pc, sl
    sub     sp, sp, #20
    str     r1, [sp, #12]
    ldr     r3, [sl, r1]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L4224
    ldr     r3, .L4928
    ldr     r2, [sl, r3]
    ldr     r3, .L492c
    ldr     ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4930
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
    ldr     r2, .L4934
    ldr     r3, .L4938
    lsr     r0, r0, #4
    str     r2, [sp, #4]
    ldr     r2, [sl, r3]
    lsl     r3, r0, #16
    lsr     r7, r3, #16
    ldr     r1, .L493c
    ldr     r3, [sp, #4]
    ldr     fp, .L4940
    ldr     r8, [sl, r1]
    ldr     r5, [sl, r3]
    ldr     r3, [sl, fp]
    and     ip, ip, #3
    ldrb    r6, [r2]
    strb    ip, [r3]
    ldrh    r2, [r8]
    ldr     r3, .L4944
    ldr     r9, .L4948
    cmp     r2, r7
    ldr     r2, [sl, r3]
    ldr     r3, .L494c
    ldr     r1, [sl, r9]
    rsb     r0, r0, r0, lsl #5
    str     lr, [r2]
    ldr     r2, [sl, r3]
    sub     r0, lr, r0, lsl #1
    and     r4, r4, r6
    strb    r4, [r5]
    strb    r0, [r1]
    strh    r7, [r2]
    beq     .L40c8
    strh    r7, [r8]
    bl      nrio_load_zone
.L40b4:
    ldr     r1, [sp, #12]
    ldr     r3, [sl, r1]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L4238
.L40c8:
    ldr     r2, .L4950
    ldr     r4, [sl, r9]
    str     r2, [sp, #8]
    ldr     r6, [sl, r2]
    ldrb    r2, [r4]
    ldrb    r3, [r6]
    cmp     r2, r3
    beq     .L42e0
.L40e8:
    bl      nrio_flush_block
    cmp     r0, #0
    bne     .L4758
    bl      nrio_allocate_free_block
    ldr     r5, .L4954
    ldr     r3, [sl, r5]
    cmp     r0, #0xff
    strb    r0, [r3]
    moveq   r4, #9
    beq     .L42d0
    ldr     r3, .L4958
    ldr     r2, [sl, r3]
    ldr     r1, [r2]
    mvn     r3, #5
    strb    r3, [r0, r1]
    ldr     r3, .L495c
    ldrb    r0, [r4]
    ldr     r2, [sl, r3]
    strb    r0, [r6]
    ldr     r8, .L4960
    ldr     r1, [r2]
    ldr     r3, [sl, r8]
    ldrb    r2, [r0, r1]
    ldr     lr, .L4934
    str     lr, [sp, #4]
    strb    r2, [r3]
    ldr     r3, [sl, r8]
    ldrb    r2, [r3]
    cmp     r2, #0xff
    ldr     ip, .L4964
    beq     .L4318
.L4164:
    ldr     r3, .L4964
    ldr     r2, .L4934
    ldr     r7, [sl, r3]
    ldr     r6, [sl, r2]
    ldr     r5, .L4954
    mov     r4, #2
.L417c:
    ldr     r3, [sl, r8]
    ldr     r2, [sl, r5]
    ldrb    r0, [r3]
    ldrb    r1, [r2]
    ldrb    r3, [r6]
    ldrb    r2, [r7]
    bl      nrio_copy_nand_pages
    cmp     r0, #0
    beq     .L48b4
    bl      nrio_replace_pending_block
    sub     r3, r4, #1
    cmp     r0, #0
    and     r4, r3, #0xff
    bne     .L4758
    cmp     r4, #0xff
    bne     .L417c
.L41bc:
    ldr     r2, [sl, fp]
    ldrb    r3, [r2]
    cmp     r3, #0
    rsb     r3, r3, #4
    and     r6, r3, #0xff
    bne     .L4250
    ldr     r7, .L4968
    ldr     r4, [sl, r7]
    ldr     r3, [r4]
    cmp     r6, r3
    bhi     .L4658
.L41e8:
    ldr     r8, .L496c
.L41ec:
    ldr     r1, [sp, #8]
    ldr     r3, [sp, #4]
    ldr     r2, [sl, r1]
    ldr     ip, [sl, r3]
    ldr     r3, .L4954
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4970
    ldrb    r0, [r2]
    ldr     r2, [sl, r3]
    ldrb    r1, [ip]
    strb    r4, [r2, #11]
    bl      nrio_select_physical_page
    b       .L426c
.L4224:
    ldr     fp, .L4940
    ldr     r3, [sl, fp]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L44f0
.L4238:
    ldr     r3, [sl, fp]
    ldrb    r3, [r3]
    cmp     r3, #0
    rsbne   r3, r3, #4
    andne   r6, r3, #0xff
    beq     .L4374
.L4250:
    ldr     r1, [sp, #12]
    ldr     r3, [sl, r1]
    ldrb    r2, [r3]
    cmp     r2, #0
    bne     .L439c
    ldr     r8, .L496c
    ldr     r7, .L4968
.L426c:
    ldr     r4, [sl, fp]
    ldr     r5, [sl, r8]
    ldrb    r3, [r4]
    mov     r0, #5
    strb    r3, [r5]
    bl      nrio_page_operation
    ldr     r3, [sl, r7]
    ldr     r1, [r3]
    cmp     r1, #1
    beq     .L4580
.L4294:
    ldr     lr, [sl, fp]
    ldrb    r3, [lr]
    cmp     r3, #3
    beq     .L4760
    ldr     r2, [sp, #12]
    ldr     r0, [sl, r7]
    ldr     ip, [sl, r2]
    add     r3, r3, #1
    mov     r2, #0
    sub     r1, r1, #1
    and     r3, r3, #3
    str     r1, [r0]
    strb    r3, [lr]
    strb    r2, [ip]
    mov     r4, r2
.L42d0:
    mov     r0, r4
    add     sp, sp, #20
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L42e0:
    ldr     r3, .L4934
    ldr     ip, .L4964
    str     r3, [sp, #4]
    ldr     r2, [sl, r3]
    ldr     r1, [sl, ip]
    ldrb    r0, [r2]
    ldrb    r3, [r1]
    cmp     r0, r3
    bcc     .L40e8
    ldr     r8, .L4960
    ldr     r3, [sl, r8]
    ldrb    r2, [r3]
    cmp     r2, #0xff
    bne     .L4164
.L4318:
    ldr     r1, [sl, ip]
    ldrb    r3, [r1]
    cmp     r3, #0
    bne     .L4164
    ldr     r2, [sp, #4]
    ldr     r3, [sl, r2]
    ldrb    r2, [r3]
    cmp     r2, #0
    beq     .L4164
    mov     r5, r1
    mov     r4, #2
.L4344:
    mov     r0, #0
    bl      nrio_write_page_marker
    cmp     r0, #0
    beq     .L48f4
    bl      nrio_replace_pending_block
    sub     r3, r4, #1
    cmp     r0, #0
    and     r4, r3, #0xff
    bne     .L4758
    cmp     r4, #0xff
    bne     .L4344
    b       .L41bc
.L4374:
    ldr     r2, .L4950
    ldr     r9, .L4948
    str     r2, [sp, #8]
    ldr     r6, [sl, r2]
    ldr     r4, [sl, r9]
    ldrb    r3, [r6]
    ldrb    r2, [r4]
    cmp     r2, r3
    bne     .L40e8
    b       .L42e0
.L439c:
    ldr     r3, .L4960
    ldr     r2, [sl, r3]
    ldrb    r0, [r2]
    cmp     r0, #0xff
    beq     .L47ec
    ldr     r2, .L4934
    str     r2, [sp, #4]
    ldr     r4, [sl, r2]
    ldrb    r1, [r4]
    bl      nrio_select_physical_page
    mov     r0, #1
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0
    bne     .L46cc
    ldr     r2, [sl, fp]
    ldr     r8, .L496c
    ldrb    r1, [r2]
    ldr     r3, [sl, r8]
    cmp     r1, #0
    strb    r0, [r3]
    beq     .L4420
    mov     r4, r3
    mov     r5, r2
.L43fc:
    mov     r0, #3
    bl      nrio_page_operation
    ldrb    r3, [r4]
    ldrb    r1, [r5]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r1, r3
    strb    r3, [r4]
    bhi     .L43fc
.L4420:
    ldr     r7, .L4968
    ldr     r3, [sl, r7]
    ldr     r2, [r3]
    cmp     r6, r2
    bls     .L4470
    add     r3, r1, #1
    and     r3, r3, #0xff
    ldr     r2, [sl, r8]
    cmp     r3, #3
    strb    r3, [r2]
    bhi     .L4470
    mov     r4, r2
.L4450:
    mov     r0, #3
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L4450
.L4470:
    ldr     r3, [sp, #4]
    ldr     ip, [sl, r3]
    ldr     r3, .L4950
    ldr     r2, [sl, r3]
    ldr     r3, .L4954
    ldrb    r4, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4970
    ldrb    r0, [r2]
    ldr     r2, [sl, r3]
    ldrb    r1, [ip]
    strb    r4, [r2, #11]
    bl      nrio_select_physical_page
    ldr     r0, [sl, fp]
    ldrb    r2, [r0]
    ldr     r1, [sl, r8]
    mov     r3, #0
    cmp     r2, #0
    strb    r3, [r1]
    beq     .L426c
    mov     r4, r1
    mov     r5, r0
.L44c8:
    mov     r0, #6
    bl      nrio_page_operation
    ldrb    r3, [r4]
    ldrb    r2, [r5]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r2, r3
    strb    r3, [r4]
    bhi     .L44c8
    b       .L426c
.L44f0:
    ldr     lr, .L4934
    ldr     r2, .L4938
    str     lr, [sp, #4]
    ldr     ip, [sl, lr]
    ldr     r1, [sl, r2]
    ldrb    r3, [ip]
    ldrb    r0, [r1]
    add     r3, r3, #1
    and     r3, r3, r0
    cmp     r3, #0
    strb    r3, [ip]
    bne     .L4238
    ldr     r9, .L4948
    ldr     ip, [sl, r9]
    ldrb    r2, [ip]
    mov     r1, #0x84000003
    add     r2, r2, #1
    and     r2, r2, #0xff
    add     r1, r1, #0x210000
    lsr     r0, r2, #1
    add     r1, r1, #0x840
    umull   lr, r3, r1, r0
    lsr     r3, r3, #4
    rsb     r3, r3, r3, lsl #5
    sub     r2, r2, r3, lsl #1
    and     r2, r2, #0xff
    cmp     r2, #0
    strb    r2, [ip]
    bne     .L4238
    ldr     r3, .L493c
    ldr     r1, [sl, r3]
    ldrh    r2, [r1]
    add     r2, r2, #1
    strh    r2, [r1]
    bl      nrio_load_zone
    b       .L40b4
.L4580:
    cmp     r6, #1
    bls     .L4294
    ldr     r8, .L4960
    ldr     r3, [sl, r8]
    ldrb    r2, [r3]
    cmp     r2, #0xff
    beq     .L4874
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r5]
    bhi     .L45d8
    mov     r4, r5
.L45b8:
    mov     r0, #6
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L45b8
.L45d8:
    bl      nrio_set_spare_markers
    mov     r0, #4
    bl      nrio_page_operation
    bl      nrio_decode_nand_status
    cmp     r0, #0
    bne     .L474c
.L45f0:
    ldr     r3, .L4964
    ldr     ip, [sl, r3]
    ldr     r3, .L4974
    ldrb    r2, [ip]
    ldr     r1, [sl, r3]
    add     r2, r2, #1
    ldrb    r0, [r1]
    and     r2, r2, #0xff
    cmp     r0, r2
    strb    r2, [ip]
    beq     .L48c0
.L461c:
    ldr     ip, [sl, fp]
    ldr     r0, [sl, r7]
    ldr     r1, [sp, #12]
    ldrb    r3, [ip]
    ldr     r2, [r0]
    ldr     lr, [sl, r1]
    add     r3, r3, #1
    mov     r1, #0
    sub     r2, r2, #1
    and     r3, r3, #3
    mov     r4, r1
    str     r2, [r0]
    strb    r3, [ip]
    strb    r1, [lr]
    b       .L42d0
.L4658:
    ldr     r3, [sl, r8]
    ldrb    r0, [r3]
    cmp     r0, #0xff
    beq     .L41e8
    ldr     lr, [sp, #4]
    ldr     r3, [sl, lr]
    ldrb    r1, [r3]
    bl      nrio_select_physical_page
    mov     r0, #1
    bl      nrio_page_operation
    bl      nrio_check_spare_markers
    cmp     r0, #0
    bne     .L41e8
    ldr     r8, .L496c
    ldrb    r3, [r4]
    ldr     r2, [sl, r8]
    cmp     r3, #3
    strb    r3, [r2]
    bhi     .L41ec
    mov     r4, r2
.L46a8:
    mov     r0, #3
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L46a8
    b       .L41ec
.L46cc:
    ldr     r3, .L4950
    ldr     r2, [sl, r3]
    ldr     r3, .L4954
    ldrb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4970
    ldrb    r0, [r2]
    ldr     r2, [sl, r3]
    ldrb    r1, [r4]
    strb    ip, [r2, #11]
    bl      nrio_select_physical_page
    ldr     r0, [sl, fp]
    ldr     r8, .L496c
    ldrb    r2, [r0]
    ldr     r1, [sl, r8]
    mov     r3, #0
    cmp     r2, #0
    strb    r3, [r1]
    beq     .L4744
    mov     r4, r1
    mov     r5, r0
.L4720:
    mov     r0, #9
    bl      nrio_page_operation
    ldrb    r3, [r4]
    ldrb    r2, [r5]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r2, r3
    strb    r3, [r4]
    bhi     .L4720
.L4744:
    ldr     r7, .L4968
    b       .L426c
.L474c:
    bl      nrio_replace_pending_block
    cmp     r0, #0
    beq     .L45f0
.L4758:
    mov     r4, r0
    b       .L42d0
.L4760:
    bl      nrio_set_spare_markers
    mov     r0, #4
    bl      nrio_page_operation
    bl      nrio_decode_nand_status
    cmp     r0, #0
    moveq   r4, r0
    bne     .L4908
.L477c:
    ldr     r3, .L4964
    ldr     ip, [sl, r3]
    ldr     r3, .L4974
    ldrb    r2, [ip]
    ldr     r1, [sl, r3]
    add     r2, r2, #1
    ldrb    r0, [r1]
    and     r2, r2, #0xff
    cmp     r0, r2
    strb    r2, [ip]
    beq     .L48d8
.L47a8:
    ldr     lr, [sl, fp]
    ldr     r0, [sl, r7]
    ldr     r3, [sp, #12]
    ldrb    r1, [lr]
    ldr     r2, [r0]
    ldr     ip, [sl, r3]
    cmp     r4, #0
    add     r1, r1, #1
    sub     r2, r2, #1
    and     r1, r1, #3
    mov     r3, #0
    movne   r4, #16
    moveq   r4, #0
    str     r2, [r0]
    strb    r3, [ip]
    strb    r1, [lr]
    b       .L42d0
.L47ec:
    ldr     r3, .L4950
    ldr     r2, [sl, r3]
    ldr     r3, .L4954
    ldrb    ip, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4934
    ldrb    r0, [r2]
    ldr     r2, [sl, r3]
    ldr     r3, .L4970
    ldrb    r1, [r2]
    ldr     r2, [sl, r3]
    strb    ip, [r2, #11]
    bl      nrio_select_physical_page
    ldr     r0, [sl, fp]
    ldr     r8, .L496c
    ldrb    r2, [r0]
    ldr     r1, [sl, r8]
    mov     r3, #0
    cmp     r2, #0
    strb    r3, [r1]
    beq     .L4744
    mov     r4, r1
    mov     r5, r0
.L4848:
    mov     r0, #9
    bl      nrio_page_operation
    ldrb    r3, [r4]
    ldrb    r2, [r5]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r2, r3
    strb    r3, [r4]
    bhi     .L4848
    ldr     r7, .L4968
    b       .L426c
.L4874:
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r5]
    bhi     .L45d8
    mov     r4, r5
.L4890:
    mov     r0, #9
    bl      nrio_page_operation
    ldrb    r3, [r4]
    add     r3, r3, #1
    and     r3, r3, #0xff
    cmp     r3, #3
    strb    r3, [r4]
    bls     .L4890
    b       .L45d8
.L48b4:
    ldrb    r3, [r6]
    strb    r3, [r7]
    b       .L41bc
.L48c0:
    ldr     r3, [sl, r8]
    ldrb    r0, [r3]
    cmp     r0, #0xff
    blne    nrio_release_block
    bl      nrio_commit_pending_block
    b       .L461c
.L48d8:
    ldr     r3, .L4960
    ldr     r2, [sl, r3]
    ldrb    r0, [r2]
    cmp     r0, #0xff
    blne    nrio_release_block
    bl      nrio_commit_pending_block
    b       .L47a8
.L48f4:
    ldr     lr, [sp, #4]
    ldr     r3, [sl, lr]
    ldrb    r2, [r3]
    strb    r2, [r5]
    b       .L41bc
.L4908:
    bl      nrio_replace_pending_block
    cmp     r0, #0
    moveq   r4, #1
    beq     .L477c
    mov     r4, r0
    b       .L42d0
.L4920:
    .word   nrio_got - (.Lgotpc_3fc4 + 16)
.L4924:
    .word   0x00000024
.L4928:
    .word   0x0000007c
.L492c:
    .word   0x000000a0
.L4930:
    .word   0x000000e0
.L4934:
    .word   0x000000b4
.L4938:
    .word   0x000000bc
.L493c:
    .word   0x00000040
.L4940:
    .word   0x00000080
.L4944:
    .word   0x0000002c
.L4948:
    .word   0x0000009c
.L494c:
    .word   0x00000084
.L4950:
    .word   0x000000ec
.L4954:
    .word   0x000000e4
.L4958:
    .word   0x00000054
.L495c:
    .word   0x000000b8
.L4960:
    .word   0x00000044
.L4964:
    .word   0x0000004c
.L4968:
    .word   0x000000dc
.L496c:
    .word   0x000000d4
.L4970:
    .word   0x00000074
.L4974:
    .word   0x00000068

.global nrio_ftl_write
.type nrio_ftl_write, %function
nrio_ftl_write:
    push    {r4, r5, r6, r7, r8, r9, sl, fp, lr}
.Lgotpc_497c:
    ldr     sl, .L4a60
    ldr     r3, .L4a64
    add     sl, pc, sl
    sub     sp, sp, #20
    ldr     ip, .L4a68
    str     r3, [sp, #8]
    ldr     r3, [sl, r3]
    ldr     lr, [r3]
    ldr     r3, [sl, ip]
    ldr     r4, [r3]
    add     lr, lr, #1
    cmp     lr, r4
    mov     fp, r1
    mov     ip, r2
    bhi     .L4a54
    ldr     r3, .L4a6c
    ldr     r2, [sl, r3]
    mvn     r1, #0
    str     r1, [r2]
    ldr     r3, .L4a70
    ldr     r2, .L4a74
    ldr     r3, [sl, r3]
    ldr     r2, [sl, r2]
    ldr     r1, .L4a78
    str     r3, [sp, #12]
    str     r2, [sp]
    str     r1, [sp, #4]
    lsr     r9, r0, #9
    lsr     r7, ip, #9
.L49f0:
    ldr     r2, [sp, #8]
    ldr     ip, [sp, #4]
    ldr     r3, [sl, r2]
    ldr     r1, [sp, #12]
    ldr     r2, [sl, ip]
    str     r9, [r3]
    cmp     r7, #0
    mov     r3, #1
    str     r7, [r2]
    strb    r3, [r1]
    beq     .L4a54
    mov     r6, #0
    ldr     r8, [sp]
    mov     r4, fp
    mov     r5, r6
.L4a2c:
    str     r4, [r8]
    bl      nrio_write_sector_with_mapping
    add     r5, r5, #1
    cmp     r0, #16
    moveq   r6, #1
    cmp     r5, r7
    add     r4, r4, #0x200
    bne     .L4a2c
    cmp     r6, #0
    bne     .L49f0
.L4a54:
    add     sp, sp, #20
    pop     {r4, r5, r6, r7, r8, r9, sl, fp, lr}
    bx      lr
.L4a60:
    .word   nrio_got - (.Lgotpc_497c + 16)
.L4a64:
    .word   0x0000007c
.L4a68:
    .word   0x00000020
.L4a6c:
    .word   0x00000064
.L4a70:
    .word   0x00000024
.L4a74:
    .word   0x00000018
.L4a78:
    .word   0x000000dc
