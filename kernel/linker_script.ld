ENTRY(_start)

SECTIONS {
    phys_base_addr = 0x40000000;
    . = phys_base_addr;
    __kernel_start_phys = .;
    __init_heap_begin_phys = . + (__init_heap_begin - __kernel_start) + (__init_end - .);

    .init.text : AT(phys_base_addr)  {
        *startup.o(.init.text)
        *init*.o(.text*)
        *(.init.text*)
    }

    .init.data : {
        *(.init.data*)
        *init*.o(.data*)
    }

    .init.rodata : {
        *(.init.rodata*)
        *init*.o(.rodata*)
    }

    .init.bss : {
        *(.init.bss*)
        *init*.o(.bss*)
    }

    /* .debug_info :  { KEEP(*(.debug_info*)) } */
    /* .debug_abbrev : { KEEP(*(.debug_abbrev*)) } */
    /* .debug_line : { KEEP(*(.debug_line*)) } */
    /* .debug_str : { KEEP(*(.debug_str*)) } */
    /* .debug_aranges : { KEEP(*(.debug_aranges*)) } */
    /* .debug_ranges : { KEEP(*(.debug_ranges*)) } */
    /* .debug_pubnames : { KEEP(*(.debug_pubnames*)) } */
    /* .debug_pubtypes : { KEEP(*(.debug_pubtypes*)) } */
    /* .debug_frame : { KEEP(*(.debug_frame*)) } */
    /* .eh_frame : { KEEP(*(.eh_frame*)) } */

    __init_end = .;

    . = 0xffff000000000000;
    __kernel_start = .;

    .text : AT(__init_end) {
        /* the first '*' is a wildcard that matches all file names  */
        *(.text*)
    }

    .vector : {
        *(.vector*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
    }

    .got : {
        *(.got)
    }

    .got.plt : {
        *(.got.plt)
    }

    .percpu : {
        __percpu_begin = .;
        *(.percpu*)
        __percpu_end = .;
    }

    STACK_SIZE = 0x10000;
    .stack : ALIGN(16) {
        __stack_bottom = .;
        __stack_bottom_phys = LOADADDR(.stack);
        __stack_top_phys = LOADADDR(.stack) + STACK_SIZE;

        /* 64 KiB stack */
        . += STACK_SIZE;

        __stack_top = .;
    }

    __init_heap_begin = .;

    /DISCARD/ : {
        *
    }
}
