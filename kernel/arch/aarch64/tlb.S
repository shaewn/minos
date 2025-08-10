    .text
    .global tlb_flush_addr
// void tlb_flush_addr(uint64_t va)
tlb_flush_addr:
    dsb ishst
    // virtual address, EL1, outer-shareable domain.
    tlbi vae1os, x0
    dsb ish
    isb
    ret
