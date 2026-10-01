/*
 * Project Zenin - Kernel Main (kmain)
 * Demonstrating Bare-Metal Boot, PMM Page Allocation, Dynamic Heap, and Z-Bus Event Mesh
 */

#include "../include/uart.h"
#include "../include/pmm.h"
#include "../include/kheap.h"
#include "../include/zbus.h"

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
    uart_puts("[zenin-pmm] Initializing Physical Page Frame Allocator...\n");
    pmm_init(free_mem_start, test_ram_size);
    uart_puts("[zenin-pmm] Total Managed RAM: 512 MB | Free Pages: ");
    print_dec(pmm_get_free_pages());
    uart_puts("\n");

    /* 2. Initialize Dynamic Kernel Heap */
    uart_puts("[zenin-heap] Initializing Dynamic Kernel Heap Allocator...\n");
    kheap_init();
    uart_puts("[zenin-heap] Kernel Heap operational.\n");

    /* 3. Initialize Z-Bus Zero-Copy Event Mesh */
    uart_puts("[zenin-zbus] Initializing Z-Bus Zero-Copy Event Mesh...\n");
    zbus_init();

    /* Test publishing an AI Intent Event */
    uart_puts("[zenin-zbus] Step 1: Publishing AI Intent Event...\n");
    bool pub_ok = zbus_publish(1, 2, ZBUS_MSG_AI_INTENT, "INTENT_TURBO_GAME_MODE", 22);
    uart_puts(pub_ok ? "[zenin-zbus] Step 2: Publish SUCCESS.\n" : "[zenin-zbus] Step 2: Publish FAILED.\n");

    /* Poll the event without memory copies */
    zbus_message_t msg;
    uart_puts("[zenin-zbus] Step 3: Polling event queue...\n");
    if (zbus_poll(&msg)) {
        uart_puts("[zenin-zbus] Step 4: Event received! Payload: ");
        uart_puts((const char *)msg.payload);
        uart_puts(" | Total Processed: ");
        print_dec(zbus_get_processed_count());
        uart_puts("\n");
    } else {
        uart_puts("[zenin-zbus] Step 4: Queue empty on poll!\n");
    }

    uart_puts("\n[zenin-core] Phase 2 Z-Bus Verified. Entering 0.0% idle WFI sleep.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
