#include "output.h"

#include "bspinlock.h"

bspinlock_t output_lock;

volatile unsigned int * const UART0DR = (unsigned int *) 0x09000000;

static void uart_putchar(char ch) {
    *UART0DR = (unsigned int)ch;
}

void kputstr(const char *s) {
    bspinlock_lock(&output_lock);
    while (*s) uart_putchar(*s++);
    bspinlock_unlock(&output_lock);
}
