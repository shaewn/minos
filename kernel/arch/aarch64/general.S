    .text
    .global tlb_flush
    .global yield
    .global cpu_idle_wait
    .global cpu_signal_all
tlb_flush: b .
yield:
    yield
    ret
cpu_idle_wait:
    ret
cpu_signal_all:
    ret
