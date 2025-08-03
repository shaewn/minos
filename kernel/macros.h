#ifndef KERNEL_MACROS_H_
#define KERNEL_MACROS_H_

#define KMIN(a, b) ((a) < (b) ? (a) : (b))
#define KMAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define ONES_IN_RANGE(h, l) (((1ULL << (h - l + 1)) - 1) << l)
#define EXTRACT(a, h, l) ((a & ONES_IN_RANGE(h, l)) >> l)
#define CLEAR(a, h, l) (a & ~ONES_IN_RANGE(h, l)) 
#define REPLACE(a, h, l, new_bits) (CLEAR(a, h, l) | ((new_bits) << l))

#endif
