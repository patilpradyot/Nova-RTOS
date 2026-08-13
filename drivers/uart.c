#include "uart.h"
#include "critical.h"
#include <stdint.h>

#define UART0_BASE  0x101F1000

#define UART0_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART0_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))

void uart_init(void) {
    
}

void uart_putc(char c) {
    while (UART0_FR & (1 << 5));
    UART0_DR = c;
}

void uart_puts(const char *str) {
    uint32_t saved_cpsr = irq_disable_save();

    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }
        uart_putc(*str++);
    }

    irq_restore(saved_cpsr);
}


void uart_print_int(int32_t val) {
    char buf[12];
    int i = 0;
    uint8_t neg = 0;
    uint32_t uval;

    if (val < 0) {
        neg = 1;
        uval = (uint32_t)(-val);
    } else {
        uval = (uint32_t)val;
    }

    if (uval == 0) {
        uart_putc('0');
        return;
    }

    while (uval > 0) {
        buf[i++] = '0' + (uval % 10);
        uval = uval / 10;
    }

    if (neg) {
        uart_putc('-');
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}


void uart_print_float(float val, int decimals) {
    if (val < 0.0f) {
        uart_putc('-');
        val = -val;
    }

    int32_t int_part = (int32_t)val;
    float frac = val - (float)int_part;

    uart_print_int(int_part);
    uart_putc('.');

    for (int i = 0; i < decimals; i++) {
        frac *= 10.0f;
    }

    int32_t frac_int = (int32_t)frac;

    if (decimals == 2 && frac_int < 10) {
        uart_putc('0');   // leading zero (e.g. "5.07" not "5.7")
    }

    uart_print_int(frac_int);
}