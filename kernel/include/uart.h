#ifndef ZENIN_UART_H
#define ZENIN_UART_H

#include <stdint.h>

/* PL011 PrimeCell UART Base Address (QEMU virt ARM64) */
#define UART0_BASE 0x09000000

#define UART_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_FR_TXFF (1 << 5) /* Transmit FIFO full */

static inline void uart_putc(char c) {
    /* Wait until FIFO has space */
    while (UART_FR & UART_FR_TXFF);
    UART_DR = c;
}

static inline void uart_puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            uart_putc('\r');
        }
        uart_putc(*str++);
    }
}

#endif /* ZENIN_UART_H */
