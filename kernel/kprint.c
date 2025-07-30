#include "output.h"
#include "memory.h"
#include "string.h"

#include <stdarg.h>
#include <stdint.h>

#define KPRINT_BUFFER_SIZE 4096
char kprint_buffer[KPRINT_BUFFER_SIZE];

static size_t int_to_buffer(char *buffer, char *end, uintmax_t u, int radix) {
    char tmp_buf[64];
    size_t i = 0;
    size_t len = 0;

    const char *chars;

    switch (radix) {
        case 2: {
            chars = "01";
            break;
        }

        case 8: {
            chars = "01234567";
            break;
        }

        case 16: {
            chars = "0123456789abcdef";
            break;
        }

        default:
            radix = 10;
        case 10:
            chars = "0123456789";
            break;
    }

    if (u == 0) {
        tmp_buf[i] = '0';
        len = 1;
        goto copy_over;
    }

    i = sizeof(tmp_buf);

    while (u) {
        uintmax_t dividend, remainder;

        --i;
        ++len;

        dividend = u / radix;
        remainder = u % radix;

        tmp_buf[i] = chars[remainder];

        u = dividend;
    }

copy_over:;
    if (buffer) {
        size_t to_copy = len;

        if (len > end - buffer) {
            to_copy = (size_t)(end - buffer);
        }

        copy_memory(buffer, tmp_buf + i, len);
    }

    return len;
}

void kprintv_nolock(const char *format, va_list list) {
    size_t buffer_pos = 0;
    const char *s = format;
    // Save one for null terminator.
    char *one_past_last = kprint_buffer + KPRINT_BUFFER_SIZE - 1;

    while (*s) {
        switch (*s) {
            case '%': {
                ++s;
                switch (*s++) {
                    case 'x': {
                        buffer_pos += int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                    (uintmax_t)va_arg(list, unsigned), 16);
                        break;
                    }

                    case 'd': {
                        int arg = va_arg(list, int);

                        if (arg < 0) {
                            if (kprint_buffer + buffer_pos < one_past_last) {
                                kprint_buffer[buffer_pos++] = '-';
                            }

                            arg = -arg;
                        }

                        buffer_pos += int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                    (uintmax_t)arg, 10);

                        break;
                    }

                    case 'u': {
                        buffer_pos += int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                    (uintmax_t)va_arg(list, unsigned), 10);
                        break;
                    }

                    case 'o': {
                        buffer_pos += int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                    (uintmax_t)va_arg(list, unsigned), 8);
                        break;
                    }

                    case 'b': {
                        buffer_pos += int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                    (uintmax_t)va_arg(list, unsigned), 2);
                        break;
                    }

                    case 'l': {
                        switch (*s++) {
                            case 'x': {
                                buffer_pos +=
                                    int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                  (uintmax_t)va_arg(list, unsigned long), 16);

                                break;
                            }

                            case 'd': {
                                long arg = va_arg(list, long);

                                if (arg < 0) {
                                    if (kprint_buffer + buffer_pos < one_past_last) {
                                        kprint_buffer[buffer_pos++] = '-';
                                    }

                                    arg = -arg;
                                }

                                buffer_pos += int_to_buffer(kprint_buffer + buffer_pos,
                                                            one_past_last, (uintmax_t)arg, 10);
                                break;
                            }

                            case 'u': {
                                buffer_pos +=
                                    int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                  (uintmax_t)va_arg(list, unsigned long), 10);
                                break;
                            }

                            case 'o': {
                                buffer_pos +=
                                    int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                  (uintmax_t)va_arg(list, unsigned long), 8);
                                break;
                            }

                            case 'b': {
                                buffer_pos +=
                                    int_to_buffer(kprint_buffer + buffer_pos, one_past_last,
                                                  (uintmax_t)va_arg(list, unsigned long), 2);
                                break;
                            }
                        }

                        break;
                    }

                    case 's': {
                        const char *s = va_arg(list, const char *);
                        size_t len = string_len(s);
                        if (kprint_buffer + buffer_pos + len > one_past_last) {
                            len = (size_t)(one_past_last - kprint_buffer - buffer_pos);
                        }

                        copy_memory(kprint_buffer + buffer_pos, s, len);
                        buffer_pos += len;
                        break;
                    }
                }

                break;
            }

            default: {
                // Check for room.
                if (kprint_buffer + buffer_pos < one_past_last) {
                    kprint_buffer[buffer_pos++] = *s++;
                } else {
                    goto formatted;
                }

                break;
            }
        }
    }

formatted:
    kprint_buffer[buffer_pos] = 0;
    kputstr_nolock(kprint_buffer);
}

void kprint_nolock(const char *format, ...) {
    va_list list;
    va_start(list, format);

    kprintv_nolock(format, list);

    va_end(list);
}

void kprintv(const char *format, va_list list) {
    klockout(1);
    kprintv_nolock(format, list);
    klockout(0);
}

void kprint(const char *format, ...) {
    va_list list;
    va_start(list, format);

    kprintv(format, list);

    va_end(list);
}

void enable_simd(void) {
    uint64_t cpacr;
    // See D24.2.33 of the A-profile reference manual (page 7376 in my version)
    asm volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr |= 3 << 20;
    asm volatile("msr cpacr_el1, %0" : :"r"(cpacr));
}

void init_print(void) {
    enable_simd();
}
