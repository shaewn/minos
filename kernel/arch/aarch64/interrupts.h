#ifndef AARCH64_INTERRUPTS_H_
#define AARCH64_INTERRUPTS_H_

#include "types.h"

typedef uint32_t intid_t;

#define INTID_INVALID ((intid_t)-1)

typedef void (*interrupt_handler_t)(intid_t intid, void *context);

#define IH_NO_HANDLER ((interrupt_handler_t)0)

typedef uint64_t handler_id_t;

// for SGIs or PPIs
// to remove a handler, pass IH_NO_HANDLER as handler.
void establish_private_handler(intid_t intid, interrupt_handler_t handler, void *context);

void establish_global_handler(intid_t intid, interrupt_handler_t handler, handler_id_t identifier, void *context);
void deestablish_global_handler(intid_t intid, handler_id_t identifier);

intid_t intid_ack(void);

// must be called for a level-sensitive interrupt.
void end_intid(intid_t intid);

int irqs_masked(void);

void mask_irqs(void);
void unmask_irqs(void);
void restore_irq_mask(int val);

bool is_private_interrupt(intid_t intid); 
bool is_global_interrupt(intid_t intid); 

void irq_enable_private(intid_t intid, bool level_sensitive);
void irq_disable_private(intid_t intid);

void irq_enable_shared(intid_t intid, bool level_sensitive);
void irq_disable_shared(intid_t intid);

// TODO:
// void irq_route_shared(intid_t intid, cpu_mask_t cpu_mask);

#endif // AARCH64_INTERRUPTS_H_
