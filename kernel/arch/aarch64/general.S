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
    wfe
    ret
cpu_signal_all:
    // In case, for example, a corresponding WFE is expecting some things to have completed.
    dsb ish
    sev
    ret

