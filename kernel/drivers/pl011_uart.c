#include "pl011_uart.h"
#include "memory.h"

static void write_reg(struct pl011_uart *uart, uint32_t offset,
                      uint32_t value) {
    mmio_write32((uintptr_t)uart->mmio_base + offset, value);
}

static uint32_t read_reg(struct pl011_uart *uart, uint32_t offset) {
    return mmio_read32((uintptr_t)uart->mmio_base + offset);
}

void pl011_uart_init(struct pl011_uart *uart) {
    pl011_uart_disable(uart);
    // Maximize baud rate.
    write_reg(uart, PL011_UARTIBRD, 0x01);
    write_reg(uart, PL011_UARTFBRD, 0x00);
    write_reg(uart, PL011_UARTLCR_H,
              PL011_UARTLCR_H_FEN | PL011_UARTLCR_H_WLEN_8);
    pl011_uart_enable(uart);
}

void pl011_uart_disable(struct pl011_uart *uart) {
    write_reg(uart, PL011_UARTCR, 0);
}

void pl011_uart_enable(struct pl011_uart *uart) {
    write_reg(uart, PL011_UARTCR,
              PL011_UARTCR_RXE | PL011_UARTCR_TXE | PL011_UARTCR_UARTEN);
}

void pl011_uart_putchar(struct pl011_uart *uart, int ch) {
    while (read_reg(uart, PL011_UARTFR) & PL011_UARTFR_TXFF)
        ;
    write_reg(uart, PL011_UARTDR, ch);
}

int pl011_uart_getchar(struct pl011_uart *uart) {
    while (read_reg(uart, PL011_UARTFR) & PL011_UARTFR_RXE)
        ;
    return read_reg(uart, PL011_UARTDR);
}
