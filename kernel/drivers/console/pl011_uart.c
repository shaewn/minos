#include "drivers/pl011_uart.h"

void console_pl011_uart_putch(void *ctx, int ch) {
    struct pl011_uart *uart = ctx;
    pl011_uart_putchar(uart, ch);
}

int console_pl011_uart_getch(void *ctx) {
    struct pl011_uart *uart = ctx;
    return pl011_uart_getchar(uart);
}
