#ifndef KERNEL_DIE_H_
#define KERNEL_DIE_H_

[[noreturn]] extern void die(void);

[[noreturn]] void kfatal(const char *file, const char *function, unsigned line, const char *s, ...);

#define KFATAL(...) kfatal(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#endif
