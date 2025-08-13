#ifndef _KERNEL_KCONSOLE_H_
#define _KERNEL_KCONSOLE_H_

#include "types.h"
#include "drivers/console.h"

void kswap_console(struct console_driver *new_console);

void klockout(int locked);

void kputch(int ch);
int kgetch(void);

// automatically acquires and releases a spin lock
void kputstr(const char *s);
void kputu(uintmax_t u, int radix);

void kprintv(const char *s, va_list list);
void kprint(const char *s, ...);

void kearly_print(const char *s, ...);

#define kearly_print kearly_print
#define kearly_putstr kputstr
#define kearly_putu kputu

#endif
