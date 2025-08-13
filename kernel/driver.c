#include "driver.h"
#include "spinlock.h"
#include "cpu.h"
#include "kmalloc.h"
#include "memory.h"

PERCPU_INIT LIST_HEAD(__pcpu_private_driver_list);
#define private_driver_list GET_PERCPU(__pcpu_private_driver_list)

volatile spinlock_t global_driver_lock;
LIST_HEAD(global_driver_list);

static void setup_driver(struct driver *driver) {
    if (driver->init) {
        driver->init(driver->context);
    }

    // enable_driver(driver->__id);
}

struct driver *dup_driver(struct driver *driver, struct driver *new_driver) {
    copy_memory(new_driver, driver, sizeof(*new_driver));

    new_driver->__id = (driver_id_t)new_driver;
    new_driver->__enabled = false;
    return new_driver;
}

driver_id_t register_private_driver(struct driver *in_driver) {
    struct driver *driver = dup_driver(in_driver, kmalloc2(sizeof(*in_driver), KMALLOC2_PRIVATE));
    list_add_tail(&driver->__node, &private_driver_list);

    setup_driver(driver);
    return driver->__id;
}

driver_id_t register_global_driver(struct driver *in_driver) {
    struct driver *driver = dup_driver(in_driver, kmalloc(sizeof(*in_driver)));
    spin_lock_irq(&global_driver_lock);
    list_add_tail(&driver->__node, &global_driver_list);
    spin_unlock_irq(&global_driver_lock);

    setup_driver(driver);
    return driver->__id;
}

void unregister_driver(driver_id_t id) {
    disable_driver(id);

    struct driver *driver = (struct driver *)id;
    driver->deinit(driver->context);

    kfree(driver);
}

static void unregister_interrupts(struct driver *driver) {
    for (uint32_t i = 0; i < driver->num_interrupts; i++) {
        intid_t intid = driver->interrupts[i];
        if (intid != INTID_INVALID) {
            if (is_private_interrupt(intid)) {
                establish_private_handler(intid, IH_NO_HANDLER, NULL);
            } else if (is_global_interrupt(intid)) {
                deestablish_global_handler(intid, (handler_id_t) driver->__id);
            }
        }
    }
}

static void driver_interrupt_handler(intid_t intid, void *driver_ptr) {
    struct driver *driver = driver_ptr;

    driver->handler(driver->context, intid);
}

static void register_interrupts(struct driver *driver) {
    for (uint32_t i = 0; i < driver->num_interrupts; i++) {
        intid_t intid = driver->interrupts[i];
        if (intid != INTID_INVALID) {
            if (is_private_interrupt(intid)) {
                establish_private_handler(intid, driver_interrupt_handler, driver);
            } else if (is_global_interrupt(intid)) {
                establish_global_handler(intid, driver_interrupt_handler, (handler_id_t) driver->__id, driver);
            }
        }
    }
}

void enable_driver(driver_id_t id) {
    struct driver *driver = (struct driver *)id;

    if (!driver->__enabled) {
        driver->__enabled = true;
        driver->on_enable(driver->context);
        register_interrupts(driver);
    }
}

void disable_driver(driver_id_t id) {
    struct driver *driver = (struct driver *)id;

    if (driver->__enabled) {
        driver->__enabled = false;
        driver->on_disable(driver->context);
        unregister_interrupts(driver);
    }
}
