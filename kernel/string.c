#include "string.h"

size_t string_len(const char *s) {
    size_t count = 0;

    while (*s++)
        count++;

    return count;
}

int8_t string_compare(const char *a, const char *b) {
    while (*a && *a == *b)
        a++, b++;

    return *a - *b;
}

uint8_t string_begins(const char *s, const char *pref) {
    while (*s && *s == *pref) ++s, ++pref;

    return !*pref;
}
