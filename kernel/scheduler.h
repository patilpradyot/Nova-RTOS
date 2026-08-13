#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

#define MAX_TASKS 8

extern tcb_t *current_tcb;
extern volatile uint32_t g_tick_count;

void scheduler_init(void);
void scheduler_add_task(tcb_t *tcb);
tcb_t *schedule_next(void);
void scheduler_start(void);
void task_sleep(uint32_t ticks);   // NAYA

#endif