#include "die.h"
#include "output.h"

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
