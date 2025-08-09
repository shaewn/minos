#include "memory.h"
#include "output.h"
#include "string.h"

#include <stdarg.h>
#include <stdint.h>

#define KPRINT_BUFFER_SIZE 4096
char kprint_buffer[KPRINT_BUFFER_SIZE];

#define MAX_INT_BUF 64

static size_t int_to_buffer(char *buffer, char *end, uintmax_t u, int radix,
                            size_t zero_pad) {
    char tmp_buf[MAX_INT_BUF];
    size_t i = 0;
    size_t len = 0;

    const char *chars;

    if (zero_pad > MAX_INT_BUF) {
        zero_pad = MAX_INT_BUF;
    }

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

    while (len < zero_pad) {
        --i;
        ++len;

        tmp_buf[i] = '0';
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

void kprintv_to_buffer(char *buffer, size_t max, const char *format,
                       va_list list) {
    size_t buffer_pos = 0;
    const char *s = format;
    // Save one for null terminator.
    char *one_past_last = buffer + max - 1;

    while (*s) {
        switch (*s) {
        case '%': {
            ++s;

            int pad = 0;
            int zeros = 0;
            int negate_pad = 0;

            if (*s == '-') {
                ++s;
                negate_pad = 1;
            }

            if (*s >= '0' && *s <= '9') {
                if (*s == '0') {
                    ++s;
                    zeros = 1;
                    negate_pad = 0;
                    // can't negate padding and prefix zeros.
                }

                while (*s >= '0' && *s <= '9') {
                    pad = pad * 10 + (*s - '0');
                    ++s;
                }
            }

            if (negate_pad) {
                pad *= -1;
            }

#define int_in_buf(val, base)                                                  \
    int_to_buffer(buffer + buffer_pos, one_past_last, val, base, zeros * pad)

            switch (*s++) {
            case 'x': {
                buffer_pos += int_in_buf((uintmax_t)va_arg(list, unsigned), 16);
                break;
            }

            case 'd': {
                int arg = va_arg(list, int);

                if (arg < 0) {
                    if (buffer + buffer_pos < one_past_last) {
                        buffer[buffer_pos++] = '-';
                    }

                    arg = -arg;
                }

                buffer_pos += int_in_buf((uintmax_t)arg, 10);

                break;
            }

            case 'u': {
                buffer_pos += int_in_buf((uintmax_t)va_arg(list, unsigned), 10);
                break;
            }

            case 'o': {
                buffer_pos += int_in_buf((uintmax_t)va_arg(list, unsigned), 8);
                break;
            }

            case 'b': {
                buffer_pos += int_in_buf((uintmax_t)va_arg(list, unsigned), 2);
                break;
            }

            case 'l': {
                switch (*s++) {
                case 'x': {
                    buffer_pos +=
                        int_in_buf((uintmax_t)va_arg(list, unsigned long), 16);

                    break;
                }

                case 'd': {
                    long arg = va_arg(list, long);

                    if (arg < 0) {
                        if (buffer + buffer_pos < one_past_last) {
                            buffer[buffer_pos++] = '-';
                        }

                        arg = -arg;
                    }

                    buffer_pos += int_in_buf((uintmax_t)arg, 10);
                    break;
                }

                case 'u': {
                    buffer_pos +=
                        int_in_buf((uintmax_t)va_arg(list, unsigned long), 10);
                    break;
                }

                case 'o': {
                    buffer_pos +=
                        int_in_buf((uintmax_t)va_arg(list, unsigned long), 8);
                    break;
                }

                case 'b': {
                    buffer_pos +=
                        int_in_buf((uintmax_t)va_arg(list, unsigned long), 2);
                    break;
                }
                }

                break;
            }

            case 's': {
                const char *s = va_arg(list, const char *);
                size_t len = string_len(s);
                if (buffer + buffer_pos + len > one_past_last) {
                    len = (size_t)(one_past_last - buffer - buffer_pos);
                }

                copy_memory(buffer + buffer_pos, s, len);
                buffer_pos += len;
                break;
            }

            case 'c': {
                int ch = va_arg(list, int);

                if (buffer + buffer_pos < one_past_last) {
                    buffer[buffer_pos++] = ch;
                } else {
                    goto formatted;
                }

                break;
            }
            }

            break;
        }

        default: {
            // Check for room.
            if (buffer + buffer_pos < one_past_last) {
                buffer[buffer_pos++] = *s++;
            } else {
                goto formatted;
            }

            break;
        }
        }
    }

formatted:
    buffer[buffer_pos] = 0;
}

void kprintv_nolock(const char *format, va_list list) {
    kprintv_to_buffer(kprint_buffer, KPRINT_BUFFER_SIZE, format, list);
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
