    .text
    .global set_percpu_start
    .global get_percpu_offset
// void set_percpu_start(void *ptr)
set_percpu_start:
    adrp x1, __percpu_begin
    sub x0, x0, x1
    msr tpidr_el1, x0
    ret
// uint64_t get_percpu_offset(void)
get_percpu_offset:
    mrs x0, tpidr_el1
    ret
