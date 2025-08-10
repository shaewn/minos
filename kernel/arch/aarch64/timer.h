#ifndef AARCH64_TIMER_H_
#define AARCH64_TIMER_H_

#include "types.h"

void timer_enable(void);
void timer_disable(void);

void timer_set_imask(void);
void timer_clear_imask(void);

void timer_set_counter(uint32_t val);
uint32_t timer_get_counter(void);

bool timer_check_cond(void);

uint32_t timer_gethz(void);

#endif
