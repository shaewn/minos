ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
    }

    . = ALIGN(16);
    . = 0x40200000;
    stack_top = .;
}
