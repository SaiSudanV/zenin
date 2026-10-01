#ifndef ZENIN_UART_H
#define ZENIN_UART_H

#include <stdint.h>
#include <stddef.h>

/* PL011 PrimeCell UART Base Address (QEMU virt ARM64) */
#define UART0_BASE 0x09000000

#define UART_DR    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR    (*(volatile uint32_t *)(UART0_BASE + 0x18))
#define UART_FR_TXFF (1 << 5) /* Transmit FIFO full */

static inline void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (uint32_t)(uint8_t)c;
}

static inline void uart_puts(const char *str) {
    if (!str) return;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            uart_putc('\r');
        }
        uart_putc(str[i]);
    }
}

#endif /* ZENIN_UART_H */
