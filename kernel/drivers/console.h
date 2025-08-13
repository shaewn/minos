#ifndef DRIVER_CONSOLE_H_
#define DRIVER_CONSOLE_H_

struct console_driver {
    void *ctx;
    void (*putch)(void *, int);
    int (*getch)(void *);
};

#endif
