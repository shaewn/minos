#ifndef KERNEL_MACROS_H_
#define KERNEL_MACROS_H_

#define KMIN(a, b) ((a) < (b) ? (a) : (b))
#define KMAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#endif
