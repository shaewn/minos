#ifndef _KERNEL_OUTPUT_H_
#define _KERNEL_OUTPUT_H_

#include "types.h"

void klockout(int locked);

// automatically acquires and releases a spin lock
void kputch(int ch);
void kputstr(const char *s);
void kputu(uintmax_t u, int radix);

void kputstr_nolock(const char *s);
void kputu_nolock(uintmax_t u, int radix);

#endif
