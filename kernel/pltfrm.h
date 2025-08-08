#ifndef KERNEL_PLTFRM_H_
#define KERNEL_PLTFRM_H_

#include "types.h"

#include "arch/aarch64/pltfrm.h"
#include "arch/aarch64/interrupts.h"

#define GET_PERCPU(var) (*ADDRESS_PERCPU(var))

void set_percpu_start(void *ptr);

#endif
