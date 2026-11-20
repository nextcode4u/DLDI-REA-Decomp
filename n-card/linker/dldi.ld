OUTPUT_ARCH(arm)
ENTRY(_init)

PHDRS {
    driver PT_LOAD FLAGS(7);
}

SECTIONS {
    . = 0xbf800000;
    .init : {
        *(.nrio.header)
        *(.init)
    } :driver
    .text : {
        __glue_start = .;
        *crtbegin.o(.text)
        *(SORT_BY_NAME(.nrio.[0-9]*))
        *(.text .text.*)
    } :driver
    .fini : { *(.fini) } :driver
    .rodata : { *(.nrio.strings) } :driver
    .init_array : { *(.init_array) } :driver
    .fini_array : { *(.fini_array) } :driver
    .eh_frame : { KEEP(*(.eh_frame)) } :driver
    .jcr : { KEEP(*(.jcr)) } :driver
    .got : {
        *(.nrio.got)
        __got_end = .;
    } :driver
    .data : { *(.data .data.*) } :driver
    .bss (NOLOAD) : {
        __bss_start = .;
        *crtbegin.o(.bss)
        *(SORT_BY_NAME(.nrio_bss.*))
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } :driver
    __got_relative_mftl_sectors = ABSOLUTE(mftl_sectors - nrio_got);
    __got_relative_mftl_spare = ABSOLUTE(mftl_spare - nrio_got);
    __got_relative_mftl_blocktab = ABSOLUTE(mftl_blocktab - nrio_got);
    __got_relative_mftl_freetab = ABSOLUTE(mftl_freetab - nrio_got);
    __got_relative_mftl_freeptr = ABSOLUTE(mftl_freeptr - nrio_got);
    /DISCARD/ : { *(.note.GNU-stack) *(.comment) *(.ARM.exidx*) *(.ARM.extab*) }
}
