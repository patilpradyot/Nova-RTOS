#include "scheduler.h"
#include "critical.h"
#include "uart.h"

tcb_t *current_tcb = 0;
volatile uint32_t g_tick_count = 0;

static tcb_t *ready_queue[MAX_TASKS];
static uint8_t task_count = 0;
static uint8_t current_index = 0;

extern void start_first_task(void);

static void stack_overflow_handler(tcb_t *tcb) {
    uart_puts("\n!!! STACK OVERFLOW DETECTED !!!\n");
    uart_puts("Task: ");
    uart_puts(tcb->name ? tcb->name : "(unknown)");
    uart_puts("\nSystem halted for safety.\n");

    uint32_t saved = irq_disable_save();
    while (1);   
}

void scheduler_init(void)
{
    task_count = 0;
    current_index = 0;
    current_tcb = 0;
    g_tick_count = 0;
}

void scheduler_add_task(tcb_t *tcb)
{
    if (task_count < MAX_TASKS) {
        tcb->state = TASK_READY;
        ready_queue[task_count++] = tcb;
    }
}

tcb_t *schedule_next(void)
{
    g_tick_count++;

    if (task_count == 0) {
        return current_tcb;
    }

    
    if (current_tcb) {
        if (task_check_stack_overflow(current_tcb)) {
            stack_overflow_handler(current_tcb);
            // yahan se wapas nahi aata
        }
    }

    if (current_tcb && current_tcb->state == TASK_RUNNING) {
        current_tcb->state = TASK_READY;
    }
    if (current_tcb) {
        current_tcb->cpu_time++;
    }

    for (uint8_t i = 0; i < task_count; i++) {
        tcb_t *t = ready_queue[i];
        if (t->state == TASK_SLEEPING && g_tick_count >= t->wake_tick) {
            t->state = TASK_READY;
        }
    }

    tcb_t *best = 0;
    uint8_t best_idx = current_index;

    uint8_t idx = current_index;
    for (uint8_t i = 0; i < task_count; i++) {
        idx = idx + 1;
        if (idx >= task_count) {
            idx = 0;
        }

        tcb_t *t = ready_queue[idx];

        if (t->state == TASK_READY || t->state == TASK_RUNNING) {
            if (best == 0 || t->priority > best->priority) {
                best = t;
                best_idx = idx;
            }
        }
    }

    if (best == 0) {
        best = current_tcb;
    } else {
        current_index = best_idx;
    }

    current_tcb = best;
    current_tcb->state = TASK_RUNNING;

    return current_tcb;
}

void scheduler_start(void)
{
    current_index = 0;
    current_tcb = ready_queue[0];
    current_tcb->state = TASK_RUNNING;
    start_first_task();
}

void task_sleep(uint32_t ticks)
{
    uint32_t saved = irq_disable_save();
    current_tcb->wake_tick = g_tick_count + ticks;
    current_tcb->state = TASK_SLEEPING;
    irq_restore(saved);

    while (current_tcb->state == TASK_SLEEPING);
}