#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t tick_ms);
void timer_irq_clear(void);

#endif