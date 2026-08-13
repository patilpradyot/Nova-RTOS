#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdint.h>

static inline uint32_t irq_disable_save(void) {
    uint32_t old_cpsr, new_cpsr;
    __asm volatile (
        "mrs %0, cpsr\n\t"
        "orr %1, %0, #0x80\n\t"
        "msr cpsr_c, %1\n\t"
        : "=r" (old_cpsr), "=r" (new_cpsr)
        :
        : "memory"
    );
    return old_cpsr;
}

static inline void irq_restore(uint32_t old_cpsr) {
    __asm volatile (
        "msr cpsr_c, %0\n\t"
        :
        : "r" (old_cpsr)
        : "memory"
    );
}

#endif