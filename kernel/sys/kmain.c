/*
 * Project Zenin - Kernel Main (kmain)
 * Demonstrating Bare-Metal Boot, PMM, Heap, Z-Bus, Framebuffer, AI Engine,
 * and the Extreme Multi-OS Potato Hardware Stress Test & Battery Analysis.
 */

#include "../include/uart.h"
#include "../include/pmm.h"
#include "../include/kheap.h"
#include "../include/zbus.h"
#include "../include/fb.h"
#include "../include/ai_core.h"
#include "../include/compat.h"
#include "../include/benchmark.h"
#include "../include/stress_test.h"

extern char __kernel_end;

static void print_dec(size_t val) {
    char buf[32];
    int idx = 0;
    if (val == 0) {
        uart_putc('0');
        return;
    }
    while (val > 0) {
        buf[idx++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = idx - 1; i >= 0; i--) {
        uart_putc(buf[i]);
    }
}

void kmain(void) {
    uart_puts("\n=====================================================\n");
    uart_puts("   PROJECT ZENIN: BARE-METAL GROUND-UP KERNEL        \n");
    uart_puts("   Status: CPU Booted into Exception Level (AArch64) \n");
    uart_puts("   Performance: Zero Lag | Peak Battery | 0.0% Idle  \n");
    uart_puts("   Target Idle RAM: < 200 MB | Zero Frame Drops      \n");
    uart_puts("=====================================================\n\n");

    /* 1. Initialize PMM */
    uintptr_t free_mem_start = (uintptr_t)&__kernel_end;
    size_t test_ram_size = 512 * 1024 * 1024;
    pmm_init(free_mem_start, test_ram_size);

    /* 2. Initialize Dynamic Kernel Heap */
    kheap_init();

    /* 3. Initialize Z-Bus Zero-Copy Event Mesh */
    zbus_init();

    /* 4. Initialize Native AI Engine */
    zenin_ai_init();

    /* 5. Initialize Universal Multi-OS Subsystem */
    compat_launcher_init();

    /* 6. Run Extreme Multi-OS Potato Hardware Stress Test & Battery Profiler */
    run_extreme_stress_test();

    uart_puts("\n[zenin-core] Stress Test Completed! Entering 0.0% idle WFI sleep.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
