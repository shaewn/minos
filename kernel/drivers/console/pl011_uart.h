#ifndef DRIVER_CONSOLE_PL011_UART_H_
#define DRIVER_CONSOLE_PL011_UART_H_

void console_pl011_uart_putch(void *ctx, int ch);
int console_pl011_uart_getch(void *ctx);

#endif
