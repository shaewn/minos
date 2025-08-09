#ifndef _KERNEL_OUTPUT_H_
#define _KERNEL_OUTPUT_H_

#include "types.h"

void klockout(int locked);

void kputch(int ch);

// automatically acquires and releases a spin lock
void kputstr(const char *s);
void kputu(uintmax_t u, int radix);

void kputstr_nolock(const char *s);
void kputu_nolock(uintmax_t u, int radix);

void init_print(void);

void kprintv_nolock(const char *s, va_list list);
void kprint_nolock(const char *s, ...);

void kprintv(const char *s, va_list list);
void kprint(const char *s, ...);

void kearly_print(const char *s, ...);

#define kearly_print kearly_print
#define kearly_putstr kputstr
#define kearly_putu kputu

#endif
