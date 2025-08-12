.macro do_min_save_context
    str x30, [sp, -16]
    mov x30, sp
    str x30, [sp, -8]
    bl min_save_context
.endm

    .section .vector, "ax"
    .global virtual_vector_base
    .p2align 11
virtual_vector_base:
    .fill (0x200 - (. - virtual_vector_base))
    do_min_save_context
    bl kernel_ehandler
    eret
    .fill (0x280 - (. - virtual_vector_base))
    do_min_save_context
    bl irq_handler
    eret

    .text
// Save the minimum amount of context necessary to avoid clobbering.
// The sequence listed below must be the first sequence in the vector table.
// Before bl'ing into this function, you MUST have saved sp and x30 into [sp, -8] and [sp, -16]
// respectively. Sequence:
//                      str x30, [sp, -16]
//                      mov x30, sp
//                      str x30, [sp, -8]
//                      bl min_save_context
min_save_context:
    // Get some registers to work with.
    stp x0, x1, [sp, -32]

    mrs x0, tpidr_el1
    adrp x1, __pcpu_min_context
    add x1, x1, :lo12:__pcpu_min_context
    add x0, x1, x0

    ldr x1, [sp, -24]

min_save_context_to:
    // x0 contains a struct regs *
    // original values for x30, sp, at [sp, -16]
    // original value for x0 at [sp, -32]
    // every other register is saved as is.

    stp x2, x3, [x0, 2 * 8]
    mov x3, x1
    ldr x2, [sp, -32]
    // x2 contains original x0
    // x3 contains original x1
    stp x2, x3, [x0, 0 * 8]

    ldp x2, x3, [sp, -16]
    // x2 contains x30
    // x3 contains sp
    stp x2, x3, [x0, 30 * 8]

    // x2, x3 already stored up top

    stp x4, x5, [x0, 4 * 8]
    stp x6, x7, [x0, 6 * 8]
    stp x8, x9, [x0, 8 * 8]
    stp x10, x11, [x0, 10 * 8]
    stp x12, x13, [x0, 12 * 8]
    stp x14, x15, [x0, 14 * 8]
    stp x16, x17, [x0, 16 * 8]
    stp x18, x19, [x0, 18 * 8]
    stp x20, x21, [x0, 20 * 8]
    stp x22, x23, [x0, 22 * 8]
    stp x24, x25, [x0, 24 * 8]
    stp x26, x27, [x0, 26 * 8]
    stp x28, x29, [x0, 28 * 8]

    // All registers are now free for saving the rest of the stuff.
    mrs x2, elr_el1
    mrs x3, spsr_el1

    stp x2, x3, [x0, 32 * 8]

    // restore x2, x3 in case the caller needs them to be unchanged.
    ldp x2, x3, [x0, 2 * 8]

    ret

    .global save_state_and_switch_to
save_state_and_switch_to:
    // x0 contains ptr to old_task
    // x1 contains ptr to new_task

    // WARNING: offsetof(struct task, cpu_regs) == 0
    str x30, [sp, -16]
    mov x30, sp
    str x30, [sp, -8]
    stp x0, x1, [sp, -32]
    bl min_save_context_to
    ldp x0, x1, [sp, -32]

    // Change the regs slightly, so that pc goes to what our x30 was upon entry.
    ldr x8, [x0, 30 * 8]
    str x8, [x0, 32 * 8]

    // TODO: Save fp_regs and extra_regs
    // NOTE: We could go back to C code at this point, since we saved the GPRs.

    mov x0, x1
    bl no_save_switch_to

// void restore_regs_and_eret(struct regs *)
restore_regs_and_eret:
    ldr x3, [x0, 31 * 8] // sp
    ldr x1, [x0, 33 * 8] // pstate (spsr_el1)
    and x2, x1, 1

    cbnz x2, 2f

//  returning to EL0
1:
    msr sp_el0, x3
    b 3f
//  returning to EL1
2:
    mov sp, x3
3:
// restore x0-x30

    ldp x2, x3, [x0, 2 * 8]
    ldp x4, x5, [x0, 4 * 8]
    ldp x6, x7, [x0, 6 * 8]
    ldp x8, x9, [x0, 8 * 8]
    ldp x10, x11, [x0, 10 * 8]
    ldp x12, x13, [x0, 12 * 8]
    ldp x14, x15, [x0, 14 * 8]
    ldp x16, x17, [x0, 16 * 8]
    ldp x18, x19, [x0, 18 * 8]
    ldp x20, x21, [x0, 20 * 8]
    ldp x22, x23, [x0, 22 * 8]
    ldp x24, x25, [x0, 24 * 8]
    ldp x26, x27, [x0, 26 * 8]
    ldp x28, x29, [x0, 28 * 8]
    ldr x30, [x0, 30 * 8]

    msr spsr_el1, x1

    ldr x1, [x0, 32 * 8]
    msr elr_el1, x1
    
    ldp x0, x1, [x0, 0 * 8]
    isb
    eret

    .global restore_then_eret

// void restore_then_eret(uint64_t orig_sp_el1, struct regs *regs);
restore_then_eret:
    mov sp, x0
    mov x0, x1
    b restore_regs_and_eret
