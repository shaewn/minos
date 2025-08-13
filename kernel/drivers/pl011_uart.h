#ifndef DRIVER_PL011_UART_H_
#define DRIVER_PL011_UART_H_

#include "types.h"

// Register offsets.
#define PL011_UARTDR 0x000
#define PL011_UARTRSR 0x004
#define PL011_UARTECR 0x004
#define PL011_UARTFR 0x018
#define PL011_UARTIBRD 0x24
#define PL011_UARTFBRD 0x28
#define PL011_UARTLCR_H 0x2C
#define PL011_UARTCR 0x030

#define PL011_UARTFR_TXFF 0x20
#define PL011_UARTFR_RXE 0x10
#define PL011_UARTFR_BUSY 0x8

#define PL011_UARTCR_RXE 0x200
#define PL011_UARTCR_TXE 0x100
#define PL011_UARTCR_UARTEN 0x1

#define PL011_UARTLCR_H_WLEN_8 0x60
#define PL011_UARTLCR_H_FEN 0x10

struct pl011_uart {
    void *mmio_base;
};

void pl011_uart_init(struct pl011_uart *uart);
void pl011_uart_enable(struct pl011_uart *uart);
void pl011_uart_disable(struct pl011_uart *uart);

void pl011_uart_putchar(struct pl011_uart *uart, int ch);
int pl011_uart_getchar(struct pl011_uart *uart);

#endif
