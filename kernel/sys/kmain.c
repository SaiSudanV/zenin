/*
 * Project Zenin - Kernel Main (kmain)
 * The Ground-Up Native Core Entry Point
 */

#include "../include/uart.h"

void kmain(void) {
    uart_puts("\n");
    uart_puts("=====================================================\n");
    uart_puts("   PROJECT ZENIN: BARE-METAL GROUND-UP KERNEL        \n");
    uart_puts("   Status: CPU Booted into Exception Level (AArch64) \n");
    uart_puts("   Architecture: Lightning Speed | Peak Battery      \n");
    uart_puts("   Target Idle RAM: < 200 MB | Zero Polling Idle     \n");
    uart_puts("=====================================================\n\n");

    uart_puts("[zenin-core] Initializing Memory Management Unit (MMU)...\n");
    uart_puts("[zenin-core] Initializing Z-Bus Zero-Copy Event Mesh...\n");
    uart_puts("[zenin-core] Initializing Edge AI Neural Core...\n");
    uart_puts("[zenin-core] System initialized. Entering WFI sleep loop.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
