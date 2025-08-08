#ifndef AARCH64_INTERRUPTS_H_
#define AARCH64_INTERRUPTS_H_

#include "types.h"

typedef uint32_t intid_t;

typedef void (*interrupt_handler_t)(intid_t intid);

#define IH_NO_HANDLER ((interrupt_handler_t)0)

// for SGIs or PPIs
void establish_private_handler(intid_t intid, interrupt_handler_t handler);
void establish_global_handler(intid_t intid, interrupt_handler_t handler);

interrupt_handler_t get_handler(intid_t intid);

intid_t intid_ack(void);

// must be called for a level-sensitive interrupt.
void end_intid(intid_t intid);

int irqs_masked(void);

void mask_irqs(void);
void unmask_irqs(void);

#endif // AARCH64_INTERRUPTS_H_
