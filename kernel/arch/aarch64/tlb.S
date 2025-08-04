    .text
    .global tlb_flush_addr
// void tlb_flush_addr(uint64_t va)
tlb_flush_addr:
    // virtual address, EL1, inner-shareable domain.
    tlbi vae1is, x0
    dsb ish
    isb
    ret
