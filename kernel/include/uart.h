#ifndef ZENIN_UART_H
#define ZENIN_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* PL011 PrimeCell / SBSA generic UART. Both expose the same register block
 * (data @0x00, flag @0x18, TXFF bit5 / RXFE bit4), so a single driver serves
 * QEMU virt's PL011 (base 0x09000000) and real-silicon SBSA UARTs (Ampere /
 * Graviton) alike. Only the base address differs, so it is runtime-configurable
 * (Stream C): the boot path may call uart_set_base() with the discovered base
 * before use; it defaults to the QEMU virt PL011 base. */
#define UART0_BASE 0x09000000

extern volatile uint64_t zenin_uart_base;

static inline void uart_set_base(uint64_t base) { zenin_uart_base = base; }

#define UART_DR    (*(volatile uint32_t *)(zenin_uart_base + 0x00))
#define UART_FR    (*(volatile uint32_t *)(zenin_uart_base + 0x18))
#define UART_FR_TXFF (1 << 5) /* Transmit FIFO full */
#define UART_FR_RXFE (1 << 4) /* Receive FIFO empty */

static inline void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF);
    UART_DR = (uint32_t)(uint8_t)c;
}

static inline bool uart_has_char(void) {
    return !(UART_FR & UART_FR_RXFE);
}

static inline char uart_getc(void) {
    while (UART_FR & UART_FR_RXFE);
    return (char)(UART_DR & 0xFF);
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

static inline void uart_put_hex(uint64_t val) {
    const char hex_chars[] = "0123456789ABCDEF";
    uart_putc(hex_chars[(val >> 28) & 0xF]);
    uart_putc(hex_chars[(val >> 24) & 0xF]);
    uart_putc(hex_chars[(val >> 20) & 0xF]);
    uart_putc(hex_chars[(val >> 16) & 0xF]);
    uart_putc(hex_chars[(val >> 12) & 0xF]);
    uart_putc(hex_chars[(val >> 8) & 0xF]);
    uart_putc(hex_chars[(val >> 4) & 0xF]);
    uart_putc(hex_chars[val & 0xF]);
}

static inline void uart_put_dec(size_t val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    char buf[32];
    int idx = 0;
    while (val > 0) {
        buf[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (idx > 0) {
        uart_putc(buf[--idx]);
    }
}

#endif /* ZENIN_UART_H */
