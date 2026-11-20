OUTPUT_ARCH(arm)
ENTRY(nrio_raw_startup)

PHDRS {
    driver PT_LOAD FLAGS(7);
}

SECTIONS {
    . = 0xbf800000;
    .header : { KEEP(*(.nrio.raw.header)) } :driver
    .text : { __glue_start = .; *(.text .text.*) } :driver
    .rodata : { *(.rodata .rodata.*) } :driver
    . = ALIGN(4);
    .got : {
        __got_start = .;
        *(.got .got.*)
        __got_end = .;
    } :driver
    .data : { *(.data .data.*) } :driver
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } :driver
    /DISCARD/ : { *(.comment .note.GNU-stack .ARM.exidx* .ARM.extab*) }

    ASSERT(SIZEOF(.header) == 128, "Invalid DLDI header size")
    ASSERT(__bss_end - nrio_raw_header <= 8192, "Driver exceeds its DLDI allocation")
}
