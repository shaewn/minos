#ifndef _KERNEL_OUTPUT_H_
#define _KERNEL_OUTPUT_H_

// automatically acquires and releases a spin lock
void kputstr(const char *s);

#endif
