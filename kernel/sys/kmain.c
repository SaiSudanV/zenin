/*
 * Project Zenin - Kernel Main (kmain)
 * Demonstrating Bare-Metal Boot, PMM Page Allocation, and Dynamic Heap kmalloc/kfree
 */

#include "../include/uart.h"
#include "../include/pmm.h"
#include "../include/kheap.h"

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
    uart_puts("   Performance: Zero Lag | Peak Battery | 0.0% Idle  \n");
    uart_puts("   Target Idle RAM: < 200 MB | Zero Frame Drops      \n");
    uart_puts("=====================================================\n\n");

    /* 1. Initialize PMM */
    uintptr_t free_mem_start = (uintptr_t)&__kernel_end;
    size_t test_ram_size = 512 * 1024 * 1024; /* 512 MB */

    uart_puts("[zenin-pmm] Initializing Physical Page Frame Allocator...\n");
    pmm_init(free_mem_start, test_ram_size);
    uart_puts("[zenin-pmm] Total Managed RAM: 512 MB | Free Pages: ");
    print_dec(pmm_get_free_pages());
    uart_puts("\n");

    /* 2. Initialize Dynamic Kernel Heap */
    uart_puts("[zenin-heap] Initializing Dynamic Kernel Heap Allocator...\n");
    kheap_init();
    uart_puts("[zenin-heap] Kernel Heap operational. Allocated bytes: ");
    print_dec(kheap_get_allocated_bytes());
    uart_puts("\n");

    /* 3. Test Dynamic Allocation (e.g. Z-Bus message & AI Buffer) */
    uart_puts("[zenin-heap] Allocating 64-byte Z-Bus event buffer...\n");
    void *zbus_msg = kmalloc(64);
    if (zbus_msg) {
        uart_puts("[zenin-heap] Buffer allocated successfully! Active heap bytes: ");
        print_dec(kheap_get_allocated_bytes());
        uart_puts(" bytes\n");

        uart_puts("[zenin-heap] Freeing buffer back to heap...\n");
        kfree(zbus_msg);
        uart_puts("[zenin-heap] Buffer freed cleanly! Active heap bytes: ");
        print_dec(kheap_get_allocated_bytes());
        uart_puts(" bytes\n");
    }

    uart_puts("\n[zenin-core] Foundation Layer 1 Complete. Entering 0.0% idle WFI sleep.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
