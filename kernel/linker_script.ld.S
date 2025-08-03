ENTRY(_start)

#include "arch/aarch64/pltfrm.h"

SECTIONS {
    phys_base_addr = 0x40000000;
    . = phys_base_addr;
    __kernel_start_phys = .;

    .init.text : AT(phys_base_addr)  {
        *startup.o(.init.text)
        *init*.o(.text*)
        *(.init.text*)
    }

    .init.vector : {
        *(.init.vector)
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

    . = ALIGN(PAGE_SIZE);
    __init_end = .;
    __kernel_end_phys = . + (__kernel_end - __kernel_start);
    __init_heap_begin_phys = . + (__init_heap_begin - __kernel_start);

    . = 0xffff000000000000;
    __kernel_start = .;

    __readonly_begin_phys = LOADADDR(.text);

    .text : AT(__init_end) {
        *(.text*)
    }

    .vector : {
        *(.vector*)
    }


    .rodata : {
        *(.rodata*)
    }

    __readonly_end_phys = LOADADDR(.data);

    . = ALIGN(PAGE_SIZE);

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

    __kernel_end = .;
    __init_heap_begin = .;

    /DISCARD/ : {
        *
    }
}
