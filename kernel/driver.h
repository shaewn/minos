#ifndef AARCH64_DRIVER_H_
#define AARCH64_DRIVER_H_

#include "pltfrm.h"
#include "list.h"

#define DRIVER_NAME_MAX 128
#define DRIVER_MAX_INTERRUPTS 16

typedef uint64_t driver_id_t;

struct driver {
    struct list_head node;
    char name[DRIVER_NAME_MAX];
    intid_t interrupts[DRIVER_MAX_INTERRUPTS];
    void *context;

    void (*init)(void *context);
    void (*deinit)(void *context);
    void (*on_enable)(void *context);
    void (*on_disable)(void *context);
    void (*handler)(void *context, intid_t intid);

    // internal
    driver_id_t __id;
    bool __enabled;
};

driver_id_t register_private_driver(struct driver *driver);
driver_id_t register_global_driver(struct driver *driver);

void unregister_driver(driver_id_t driver);

void enable_driver(driver_id_t driver);
void disable_driver(driver_id_t driver);

#endif
