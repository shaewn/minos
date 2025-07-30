#ifndef KERNEL_STRING_H_
#define KERNEL_STRING_H_

#include "types.h"

size_t string_len(const char *s);
int8_t string_compare(const char *a, const char *b);
uint8_t string_begins(const char *s, const char *pref);

void copy_string(char *dst, const char *s);

#endif
