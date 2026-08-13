#include "task.h"

void task_create(tcb_t *tcb, void (*task_func)(void), uint32_t *stack, uint8_t priority, const char *name) {
    uint32_t *sp = stack + STACK_SIZE;   

    

    *(--sp) = 0;                          // LR

    for (int i = 0; i < 13; i++) {        // r12...r0
        *(--sp) = 0;
    }

    *(--sp) = (uint32_t)task_func;        // PC
    *(--sp) = 0x1F;                       // CPSR (last write, lowest addr of frame)

    
    stack[0] = STACK_CANARY_VALUE;

    tcb->stack_ptr     = sp;
    tcb->stack_base     = stack;
    tcb->priority        = priority;
    tcb->base_priority  = priority;
    tcb->state           = TASK_READY;
    tcb->wake_tick       = 0;
    tcb->name            = name;
    tcb->next             = 0;
}


uint8_t task_check_stack_overflow(tcb_t *tcb) {
    if (tcb->stack_base[0] != STACK_CANARY_VALUE) {
        return 1;   // overflow detected!
    }
    return 0;
}