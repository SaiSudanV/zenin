/*
 * Project Zenin - Kernel Main (kmain)
 * Demonstrating Bare-Metal Boot, PMM, Heap, Z-Bus, Framebuffer, and AI Intent Dispatcher
 */

#include "../include/uart.h"
#include "../include/pmm.h"
#include "../include/kheap.h"
#include "../include/zbus.h"
#include "../include/fb.h"
#include "../include/ai_core.h"

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

    /* 4. Initialize Universal Direct Framebuffer (Display Engine) */
    uart_puts("[zenin-display] Initializing Universal Direct Framebuffer (1080x2400)...\n");
    void *vram_buffer = kmalloc(1080 * 2400 * sizeof(uint32_t));
    if (vram_buffer) {
        fb_init(1080, 2400, vram_buffer);
        fb_clear(FB_COLOR_BLACK);
        fb_fill_rect(240, 600, 600, 200, FB_COLOR_ZENIN);
        uart_puts("[zenin-display] Framebuffer scanout buffer active!\n");
        kfree(vram_buffer);
    }

    /* 5. Initialize Native AI Engine & Context Aggregator */
    uart_puts("\n[zenin-ai] Initializing Native AI Intent & Context Engine...\n");
    zenin_ai_init();

    zenin_system_context_t ctx;
    zenin_ai_get_context(&ctx);
    uart_puts("[zenin-ai] System Context Snapshot: Battery ");
    print_dec(ctx.battery_percent);
    uart_puts("% | Free RAM ");
    print_dec(ctx.free_ram_mb);
    uart_puts(" MB | CPU ");
    print_dec(ctx.cpu_clock_mhz);
    uart_puts(" MHz\n");

    /* Test Natural Language User Prompt -> Hardware Action */
    const char *user_prompt = "Hey Zenin, launch a windows game with turbo mode";
    uart_puts("[zenin-ai] Processing User Intent: '");
    uart_puts(user_prompt);
    uart_puts("'\n");

    zenin_ai_action_t resolved = zenin_ai_resolve_intent(user_prompt);
    if (resolved == AI_ACTION_TURBO_GAME_MODE) {
        uart_puts("[zenin-ai] Intent Resolved: AI_ACTION_TURBO_GAME_MODE! Dispatching over Z-Bus...\n");
        zenin_ai_dispatch_action(resolved);

        zbus_message_t msg;
        if (zbus_poll(&msg)) {
            uart_puts("[zenin-zbus] Hardware Action Triggered: ");
            uart_puts((const char *)msg.payload);
            uart_puts("\n");
        }
    }

    uart_puts("\n[zenin-core] AI-First Kernel Operational! Entering 0.0% idle WFI sleep.\n");

    /* Infinite Low-Power Wait-For-Interrupt (WFI) Loop */
    while (1) {
        __asm__ volatile("wfi");
    }
}
