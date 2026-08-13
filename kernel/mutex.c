#include "mutex.h"
#include "critical.h"

extern tcb_t *current_tcb;

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner = 0;
    m->waiter_count = 0;
    for (uint8_t i = 0; i < MUTEX_MAX_WAITERS; i++) {
        m->waiters[i] = 0;
    }
}

void mutex_lock(mutex_t *m) {
    while (1) {
        uint32_t saved = irq_disable_save();

        if (!m->locked) {
            m->locked = 1;
            m->owner = current_tcb;
            irq_restore(saved);
            return;   // lock mil gaya
        }

        
        if (m->owner && current_tcb->priority > m->owner->priority) {
            m->owner->priority = current_tcb->priority;
        }

        /* Apne aap ko waiter list mein register karo (agar pehle se nahi hai) */
        uint8_t already_waiting = 0;
        for (uint8_t i = 0; i < m->waiter_count; i++) {
            if (m->waiters[i] == current_tcb) {
                already_waiting = 1;
                break;
            }
        }
        if (!already_waiting && m->waiter_count < MUTEX_MAX_WAITERS) {
            m->waiters[m->waiter_count++] = current_tcb;
        }

       
        current_tcb->state = TASK_BLOCKED;

        irq_restore(saved);

        
        while (current_tcb->state == TASK_BLOCKED);

        
    }
}

void mutex_unlock(mutex_t *m) {
    uint32_t saved = irq_disable_save();

    if (m->owner == current_tcb) {
        current_tcb->priority = current_tcb->base_priority;   // original priority restore
        m->locked = 0;
        m->owner = 0;

        
        for (uint8_t i = 0; i < m->waiter_count; i++) {
            if (m->waiters[i]->state == TASK_BLOCKED) {
                m->waiters[i]->state = TASK_READY;
            }
        }
        m->waiter_count = 0;
    }

    irq_restore(saved);
}