#include "die.h"
#include "kconsole.h"

void kprint_nolock(const char *f, ...);
void kprintv_nolock(const char *s, va_list l);

[[noreturn]] void kfatal(const char *file, const char *function, unsigned line, const char *s, ...) {
    va_list list;
    va_start(list, s);
    klockout(1);
    kprint_nolock("%s in %s at line %u: ", file, function, line);
    kprintv_nolock(s, list);
    klockout(0);
    va_end(list);

    die();
}
