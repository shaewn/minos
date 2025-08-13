#include "kconsole.h"

#include "spinlock.h"
#include "memory.h"
#include "pltfrm.h"
#include "drivers/console.h"

volatile spinlock_t output_lock;

struct buffer_ctx {
    char buffer[16384];
    int offset;
    int full;
} buffer_ctx;

void buffer_putch(void *ctx, int ch) {
    struct buffer_ctx *b = ctx;
    b->buffer[b->offset] = ch;
    b->offset++;
    if (b->offset >= ARRAY_LEN(b->buffer)) {
        b->offset = 0;
        b->full = 1;
    }
}

int buffer_getch(void *ctx) {
    return 0;
}

struct console_driver buffer_driver = {
    &buffer_ctx,
    buffer_putch,
    buffer_getch
};

static struct console_driver *active_console = &buffer_driver;

void kswap_console(struct console_driver *new_console) {
    klockout(1);

    if (active_console == &buffer_driver) {
        // Forward all the output.
        struct buffer_ctx *b = active_console->ctx;
        if (b->full) {
            for (int i = b->offset; i < ARRAY_LEN(b->buffer); i++) {
                new_console->putch(new_console->ctx, b->buffer[i]);
            }

            for (int i = 0; i < b->offset; i++) {
                new_console->putch(new_console->ctx, b->buffer[i]);
            }
        } else {
            for (int i = 0; i < b->offset; i++) {
                new_console->putch(new_console->ctx, b->buffer[i]);
            }
        }
        b->offset = 0;
        b->full = 0;
    }

    active_console = new_console;

    klockout(0);
}

void klockout(int locked) {
    if (locked) {
        spin_lock_irq(&output_lock);
    } else {
        spin_unlock_irq(&output_lock);
    }
}

void kputch(int ch) {
    active_console->putch(active_console->ctx, ch);
}

int kgetch(void) {
    return active_console->getch(active_console->ctx);
}

void kputstr(const char *s) {
    klockout(1);
    kputstr_nolock(s);
    klockout(0);
}

void kputstr_nolock(const char *s) {
    while (*s) kputch(*s++);
}

void kputu(uintmax_t u, int radix) {
    klockout(1);
    kputu_nolock(u, radix);
    klockout(0);
}

void kputu_nolock(uintmax_t u, int radix) {
    if (u == 0) {
        kputstr_nolock("0");
    }

    const char *s;
    int use_base;
    switch (radix) {
        case 2:
            use_base = 2;
            s = "01";
            break;
        case 8:
            use_base = 8;
            s = "01234567";
            break;
        case 16:
            use_base = 16;
            s = "0123456789abcdef";
            break;
        default:
            use_base = 10;
            s = "0123456879";
            break;
    }

    // Should be sufficiently large for any uintmax_t
    char buf[64];
    size_t i;
    set_memory(buf, 0, sizeof(buf));

    for (i = 63; u != 0; i--) {
        uint64_t dividend, remainder;
        dividend = u / use_base;
        remainder = u % use_base;

        buf[i - 1] = s[remainder];

        u = dividend;
    }

    kputstr_nolock(buf + i);
}
