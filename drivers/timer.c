#include "timer.h"

#define TIMER0_BASE   0x101E2000UL

#define TIMER_LOAD    (*(volatile uint32_t *)(TIMER0_BASE + 0x00))
#define TIMER_CTRL    (*(volatile uint32_t *)(TIMER0_BASE + 0x08))
#define TIMER_INTCLR  (*(volatile uint32_t *)(TIMER0_BASE + 0x0C))

#define CTRL_32BIT     (1 << 1)
#define CTRL_INTEN     (1 << 5)
#define CTRL_PERIODIC  (1 << 6)
#define CTRL_ENABLE    (1 << 7)

#define TIMER_CLK_HZ   1000000UL

#define VIC_BASE       0x10140000UL
#define VIC_INTENABLE  (*(volatile uint32_t *)(VIC_BASE + 0x10))
#define TIMER01_IRQ_BIT (1 << 4)

void timer_init(uint32_t tick_ms) {
    TIMER_CTRL = 0;
    TIMER_LOAD = (TIMER_CLK_HZ / 1000UL) * tick_ms;
    TIMER_CTRL = CTRL_32BIT | CTRL_PERIODIC | CTRL_INTEN | CTRL_ENABLE;
    VIC_INTENABLE = TIMER01_IRQ_BIT;
}

void timer_irq_clear(void) {
    TIMER_INTCLR = 1;
}