#ifndef AARCH64_INTERRUPTS_H_
#define AARCH64_INTERRUPTS_H_

#include "types.h"
#include "sgis.h"

#define INTID_INVALID ((intid_t)-1)
#define IH_NO_HANDLER ((interrupt_handler_t)0)

typedef uint32_t intid_t;

typedef void (*interrupt_handler_t)(intid_t intid, void *context);

typedef uint64_t handler_id_t;

struct sgi_data {
    void *payload;
    cpu_t sender;
};

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

// 0 on success, -1 on failure.
int accept_sgi(intid_t intid, struct sgi_data *data_buf);
int end_sgi(intid_t intid);

void send_all_sgi(intid_t intid, void *payload);
void send_sgi(cpu_t target, intid_t intid, void *payload);

// TODO:
// void irq_route_shared(intid_t intid, cpu_mask_t cpu_mask);

#endif // AARCH64_INTERRUPTS_H_
