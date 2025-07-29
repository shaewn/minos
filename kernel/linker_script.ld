ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text : {
        /* '*' is a wildcard that matches all file names  */
        *startup.o(.text) /* Make sure to put the entry point at the beginning of the text section. */
        *(.text*)
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

    .percpu : {
        *(.percpu*)
    }

    . = ALIGN(16); /* Make sure the stack is 16-byte aligned */
    /* 64 KiB stack */
    . = . + 0x10000;
    __stack_top = .;
}
