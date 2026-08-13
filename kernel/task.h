#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define STACK_SIZE 1024        
#define STACK_CANARY_VALUE 0xDEADBEEF

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_SUSPENDED
} task_state_t;

typedef struct tcb {
    uint32_t *stack_ptr;
    uint32_t *stack_base;     
    task_state_t state;
    uint8_t priority;
    uint8_t base_priority;
    uint32_t cpu_time;
    uint32_t wake_tick;
    const char *name;          
    struct tcb *next;
} tcb_t;

void task_create(tcb_t *tcb, void (*task_func)(void), uint32_t *stack, uint8_t priority, const char *name);
void cpu_switch_context(uint32_t **old_sp, uint32_t **new_sp);
uint8_t task_check_stack_overflow(tcb_t *tcb);   

#endif