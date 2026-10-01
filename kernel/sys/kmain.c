/*
 * Project Zenin - Kernel Main (kmain)
 * Demonstrating Bare-Metal Boot & Physical Memory Management (PMM)
 */

#include "../include/uart.h"
#include "../include/pmm.h"

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
    uart_puts("\n");
    uart_puts("=====================================================\n");
    uart_puts("   PROJECT ZENIN: BARE-METAL GROUND-UP KERNEL        \n");
    uart_puts("   Status: CPU Booted into Exception Level (AArch64) \n");
    uart_puts("   Architecture: Lightning Speed | Peak Battery      \n");
    uart_puts("   Target Idle RAM: < 200 MB | Zero Polling Idle     \n");
    uart_puts("=====================================================\n\n");

    /* 1. Initialize PMM with 512 MB of physical RAM starting after the kernel */
    uintptr_t free_mem_start = (uintptr_t)&__kernel_end;
    size_t test_ram_size = 512 * 1024 * 1024; /* 512 MB */

    uart_puts("[zenin-pmm] Initializing Physical Page Frame Allocator...\n");
    pmm_init(free_mem_start, test_ram_size);

    uart_puts("[zenin-pmm] Total Managed Physical Pages: ");
    print_dec(pmm_get_total_pages());
    uart_puts(" (512 MB)\n");

    uart_puts("[zenin-pmm] Free Pages Available: ");
    print_dec(pmm_get_free_pages());
    uart_puts("\n");

    /* 2. Test Allocating & Freeing Physical 4KB Pages */
    uart_puts("[zenin-pmm] Allocating test page frame...\n");
    void *page1 = pmm_alloc_page();
    if (page1) {
        uart_puts("[zenin-pmm] Page allocated successfully! Free pages: ");
        print_dec(pmm_get_free_pages());
        uart_puts("\n");

        uart_puts("[zenin-pmm] Freeing test page frame...\n");
        pmm_free_page(page1);
        uart_puts("[zenin-pmm] Page freed successfully! Free pages: ");
        print_dec(pmm_get_free_pages());
        uart_puts("\n");
    }

    uart_puts("\n[zenin-core] Physical memory engine verified. Entering WFI sleep loop.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
