#include "output.h"

#include "bspinlock.h"
#include "memory.h"
#include "uart.h"

bspinlock_t output_lock;

static void uart_putchar(char ch) {
    *(volatile int *) UART_ADDR = (unsigned int)ch;
}

void klockout(int locked) {
    if (locked) {
        bspinlock_lock(&output_lock);
    } else {
        bspinlock_unlock(&output_lock);
    }
}

void kputch(int ch) {
    uart_putchar(ch);
}

void kputstr(const char *s) {
    klockout(1);
    kputstr_nolock(s);
    klockout(0);
}

void kputstr_nolock(const char *s) {
    while (*s) uart_putchar(*s++);
}

void kputu(uintmax_t u, int radix) {
    klockout(1);
    kputu_nolock(u, radix);
    klockout(0);
}

void kputu_nolock(uintmax_t u, int radix) {
    if (u == 0) {
        kputstr_nolock("0");
    }

    const char *s;
    int use_base;
    switch (radix) {
        case 2:
            use_base = 2;
            s = "01";
            break;
        case 8:
            use_base = 8;
            s = "01234567";
            break;
        case 16:
            use_base = 16;
            s = "0123456789abcdef";
            break;
        default:
            use_base = 10;
            s = "0123456879";
            break;
    }

    // Should be sufficiently large for any uintmax_t
    char buf[64];
    size_t i;
    set_memory(buf, 0, sizeof(buf));

    for (i = 63; u != 0; i--) {
        uint64_t dividend, remainder;
        dividend = u / use_base;
        remainder = u % use_base;

        buf[i - 1] = s[remainder];

        u = dividend;
    }

    kputstr_nolock(buf + i);
}
