#ifndef MUTEX_H
#define MUTEX_H

#include "task.h"

#define MUTEX_MAX_WAITERS 4

typedef struct {
    volatile uint8_t locked;
    tcb_t *owner;
    tcb_t *waiters[MUTEX_MAX_WAITERS];
    uint8_t waiter_count;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif