/*
 * Project Zenin - Microkernel Main (kmain)
 *
 * Strict Reality-Bound Engineering Compliance:
 * - ZERO game-specific code, headers, or static game payloads.
 * - Pure microkernel architecture: PMM, KHeap, Z-Bus, Framebuffer, Input Digitizer,
 *   Multi-OS Containers (Android, Windows, Linux, macOS).
 * - Pure raw input stream passthrough (Mouse, Keyboard, Gamepad, Touch).
 * - Zero hardcoded action triggers or fake interactive mocks.
 */

#include "../include/uart.h"
#include "../include/pmm.h"
#include "../include/kheap.h"
#include "../include/zbus.h"
#include "../include/fb.h"
#include "../include/ai_core.h"
#include "../include/compat.h"
#include "../include/input.h"
#include "../include/compositor.h"
#include "../include/neural_core.h"
#include "../include/shell.h"
#include "../include/abi_shim.h"
#include "../include/zgl.h"
#include "../include/zpkg.h"
#include "../include/zld.h"
#include "../include/unified_bridge.h"
#include "../include/production_test.h"
#include "../include/container_loader.h"
#include "../include/pe_loader.h"
#include "../include/hal_cpu.h"
#include "../include/irq.h"
#include "../include/boot_info.h"
#include "../include/memory_accounting.h"
#include "../include/arch/aarch64/mmu.h"

extern char __kernel_end;
extern char __kernel_start;

/* Shared UART base (see include/uart.h) — PL011/SBSA register-compatible. */
volatile uint64_t zenin_uart_base = UART0_BASE;
extern char __bss_start;
extern char __bss_end;
extern char __stack_bottom;
extern char __stack_top;

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static uint8_t hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

void kmain(uint64_t boot_info) {
    uart_puts("\n=====================================================\n");
    uart_puts("   PROJECT ZENIN: BARE-METAL GROUND-UP KERNEL        \n");
    uart_puts("   Architecture: AArch64 Bare-Metal Microkernel      \n");
    uart_puts("   Test target: QEMU virt | Cortex-A53 | 2 GiB RAM\n");
    uart_puts("   Resource targets are measured separately; no CPU/power result yet.\n");
    uart_puts(arch_mmu_identity_map_installed()
                  ? "[zenin-mmu] Zenin AArch64 stage-1 identity map and caches enabled.\n"
                  : "[zenin-mmu] Zenin identity map not installed (translation was already enabled or setup skipped).\n");
    uart_puts(arch_mmu_device_window_verified()
                  ? "[zenin-mmu] GICD/GICC/UART active descriptors are Device-nGnRnE: PASS.\n"
                  : "[zenin-mmu] GICD/GICC/UART active descriptors are not Device-nGnRnE: FAIL.\n");
    uart_puts("=====================================================\n\n");

    /* 1. Initialize Physical Memory Manager.
     * Stream C: size the PMM from the firmware memory map the UEFI stub passed
     * in x0 (boot_info) when present; fall back to the QEMU virt layout. */
    const zenin_boot_info_t *bi = (const zenin_boot_info_t *)boot_info;
    uintptr_t ram_base, ram_end, lowest_ram_base;
    bool descriptor_valid = zenin_boot_info_valid(bi);
    if (boot_info && !descriptor_valid) {
        uart_puts("[zenin-boot] Invalid firmware memory map; reason=");
        if (bi->magic != ZENIN_BOOT_INFO_MAGIC) {
            uart_puts("bad magic or truncated firmware map");
        } else {
            uart_put_dec((size_t)zenin_boot_info_error(bi));
        }
        uart_puts(" ram-count=");
        uart_put_dec((size_t)bi->ram_count);
        uart_puts(" mmio-count=");
        uart_put_dec((size_t)bi->mmio_count);
        uart_puts("; refusing unsafe PMM setup.\n");
        for (;;) __asm__ volatile("wfi");
    }
    bool have_map = descriptor_valid;
    if (have_map) {
        const zenin_boot_range_t *regions = zenin_boot_ram(bi);
        uintptr_t k_end = (uintptr_t)&__kernel_end;
        ram_base = 0;
        ram_end = 0;
        lowest_ram_base = UINTPTR_MAX;
        for (uint64_t i = 0; i < bi->ram_count; i++) {
            uintptr_t b = (uintptr_t)regions[i].base;
            uint64_t checked_end;
            if (!zenin_boot_range_end(&regions[i], &checked_end) ||
                checked_end > UINTPTR_MAX) {
                uart_puts("[zenin-boot] Firmware RAM range exceeds address width.\n");
                for (;;) __asm__ volatile("wfi");
            }
            uintptr_t e = (uintptr_t)checked_end;
            if (b < lowest_ram_base) lowest_ram_base = b;
            if (ram_end == 0 && k_end > b && k_end <= e) {
                ram_base = b;
                ram_end = e;
            }
        }
        if (ram_end == 0) {
            uart_puts("[zenin-boot] No validated firmware RAM range contains kernel end.\n");
            for (;;) __asm__ volatile("wfi");
        }
        uart_puts("[zenin-boot] PMM sized from the firmware memory map.\n");
    } else {
        ram_base = 0x40000000ULL;
        ram_end = ram_base + 2048ULL * 1024 * 1024;
        lowest_ram_base = ram_base;
        uart_puts("[zenin-boot] No valid boot-info; using QEMU virt RAM fallback.\n");
    }
    uintptr_t mem_start = (uintptr_t)&__kernel_end;
    if (mem_start < ram_base || mem_start >= ram_end) {
        uart_puts("[zenin-pmm] Unsupported or invalid RAM layout. Halting.\n");
        for (;;) __asm__ volatile("wfi");
    }
    pmm_init(mem_start, ram_end - mem_start);
    bool irq_ready = zenin_irq_init();
    uart_puts(irq_ready ? "[zenin-irq] QEMU GICv2 registers programmed; timer wake not yet verified.\n"
                        : "[zenin-irq] Timer IRQ configuration unavailable.\n");

    /* 2. Initialize Framebuffer Driver (360x640 RGB565 1:1) */
    uint32_t fb_width = 360;
    uint32_t fb_height = 640;
    size_t fb_bytes = fb_width * fb_height * sizeof(uint16_t);
    size_t fb_pages = (fb_bytes + 4095) / 4096;
    size_t free_pages_before_fb = pmm_get_free_pages();
    void *fb_mem = pmm_alloc_pages(fb_pages);
    size_t free_pages_after_fb = pmm_get_free_pages();
    uart_puts("[zenin-memory] Framebuffer request: ");
    uart_put_dec(fb_bytes);
    uart_puts(" bytes / ");
    uart_put_dec(fb_pages);
    uart_puts(" pages; PMM accounted ");
    uart_put_dec(free_pages_before_fb - free_pages_after_fb);
    uart_puts(" pages\n");
    fb_init(fb_width, fb_height, fb_mem);

    /* 3. Kernel heap storage is acquired lazily on the first kmalloc call. */

    /* 4. Layer 4: Micro-Compositor & Multi-Container Surface Manager */
    compositor_init(fb_width, fb_height);

    /* 5. Layer 5: Ultra-Low Latency Input Digitizer */
    input_init(fb_width, fb_height);

    /* 6. Layer 6: Neural Core & Battery Power Governor */
    neural_core_init();

    /* 7. Layer 7: Interactive Shell Subsystem */
    shell_init();

    /* 8. Layer 8: Multi-OS ABI Dispatch Engine */
    abi_shim_init();

    /* 9. Layer 9: Zero-Copy Package Container */
    zpkg_init();

    /* 10. Layer 10: Dynamic Symbol Linker (z-ld.so) & CRT Bridge */
    zld_init();

    /* 11. Prototype software graphics/audio API bridge */
    unified_bridge_init();

    /* 12. Microkernel Subsystem Integrity Tests (Zero Mock Code) */
    /* D012: freeze the kernel-image/.rodata/arena/vector mapping subtree
     * (Read-Only tables + W^X) before any test or application runs. The
     * heap subtree stays dynamically mappable but is XNTable-locked. */
    arch_mmu_freeze_kernel_map();
    uart_puts("[zenin-mmu] Kernel map frozen: static tables RO, heap subtree XNTable-locked.\n");
    production_test_init();
    bool smoke_tests_passed = production_run_smoke_tests();
    uart_puts(smoke_tests_passed
                  ? "[zenin-test] COMPLETE: PASS\n"
                  : "[zenin-test] COMPLETE: FAIL (one or more smoke checks failed)\n");

    /* 13. Format parser prototypes; these do not execute application binaries. */
    container_subsystem_init();
    /* Do not execute the legacy synthetic-header examples as if they were
     * real OS application compatibility tests. */
    pe_subsystem_init();

    /* Report only accounting directly derived from loaded image bounds and
     * PMM allocation counters. This is not whole-machine power/CPU telemetry. */
    size_t free_pg = pmm_get_free_pages();
    size_t total_pg = pmm_get_total_pages();
    size_t used_pg = total_pg - free_pg;
    uint64_t allocator_bytes = 0;
    uint64_t kernel_start_addr = (uintptr_t)&__kernel_start;
    uint64_t kernel_end_addr = (uintptr_t)&__kernel_end;
    uint64_t image_rw_bytes = 0, bss_bytes = 0, stack_bytes = 0;
    uint64_t accounted_bytes = 0;
    bool image_bounds_valid = kernel_end_addr > kernel_start_addr &&
        (uintptr_t)&__bss_start >= (uintptr_t)&__kernel_start &&
        (uintptr_t)&__bss_end >= (uintptr_t)&__bss_start &&
        (uintptr_t)&__stack_bottom >= (uintptr_t)&__bss_end &&
        (uintptr_t)&__kernel_end >= (uintptr_t)&__stack_bottom;
    bool accounting_valid = image_bounds_valid &&
        zenin_pages_to_bytes((uint64_t)used_pg, &allocator_bytes) &&
        zenin_os_owned_resident_bytes(kernel_start_addr, kernel_end_addr,
                                      (uint64_t)used_pg, &accounted_bytes);
    if (image_bounds_valid) {
        image_rw_bytes = (uintptr_t)&__bss_start - (uintptr_t)&__kernel_start;
        bss_bytes = (uintptr_t)&__bss_end - (uintptr_t)&__bss_start;
        stack_bytes = (uintptr_t)&__kernel_end - (uintptr_t)&__stack_bottom;
    }
    uint64_t platform_reserved_bytes = 0;
    bool platform_reserved_valid = zenin_platform_reserved_before_kernel(
        kernel_start_addr, (uint64_t)ram_base, &platform_reserved_bytes);

    uart_puts("\n=====================================================\n");
    uart_puts("   PROJECT ZENIN: KERNEL PROTOTYPE RUNNING          \n");
    uart_puts("=====================================================\n");
    uart_puts("[zenin-memory] Kernel image code+rodata+data: ");
    uart_put_dec(image_rw_bytes / 1024);
    uart_puts(" KiB; BSS: ");
    uart_put_dec(bss_bytes / 1024);
    uart_puts(" KiB; boot stack: ");
    uart_put_dec(stack_bytes / 1024);
    uart_puts(" KiB\n[zenin-memory] PMM-accounted resident pages: ");
    uart_put_dec(used_pg);
    uart_puts(" pages / ");
    uart_put_dec(allocator_bytes / 1024);
    uart_puts(" KiB\n");
    if (accounting_valid) {
        uart_puts("[zenin-memory] Accounting inputs: kernel-image=");
        uart_put_dec((size_t)((kernel_end_addr - kernel_start_addr) / 1024));
        uart_puts(" KiB; PMM-used=");
        uart_put_dec(used_pg);
        uart_puts(" pages / ");
        uart_put_dec((size_t)(allocator_bytes / 1024));
        uart_puts(" KiB\n");
        uart_puts("[zenin-memory] OS-owned resident total: ");
        uart_put_dec(accounted_bytes / 1024);
        uart_puts(" KiB (kernel image + PMM-used pages; shared counted once)\n");
    } else {
        uart_puts("[zenin-memory] OS-owned resident total: UNAVAILABLE (invalid range or arithmetic)\n");
    }
    uart_puts("[zenin-memory] Lowest usable RAM base: 0x");
    uart_put_hex64((uint64_t)lowest_ram_base);
    uart_puts("; selected PMM range base: 0x");
    uart_put_hex64((uint64_t)ram_base);
    uart_puts("; kernel load start: 0x");
    uart_put_hex64(kernel_start_addr);
    uart_puts("\n");
    if (platform_reserved_valid) {
        uart_puts("[zenin-memory] Platform-reserved span below kernel load (reported separately, not OS-owned): ");
        uart_put_dec(platform_reserved_bytes / 1024);
        uart_puts(" KiB; device MMIO is outside this span\n");
    } else {
        uart_puts("[zenin-memory] Platform-reserved span below kernel load: UNAVAILABLE (selected PMM range base is above kernel start); not included in OS-owned total\n");
    }
    if (!accounting_valid) {
        uart_puts("[zenin-memory] QEMU OS-accounted idle-RAM budget: UNAVAILABLE (accounting validation failed)\n");
    } else if (zenin_idle_ram_within_budget(accounted_bytes, 5ULL * 1024 * 1024)) {
        uart_puts("[zenin-memory] QEMU OS-accounted idle-RAM budget: PASS (<5 MiB); actual-device measurement requires target hardware\n");
    } else {
        uart_puts("[zenin-memory] QEMU OS-accounted idle-RAM budget: FAIL (>=5 MiB)\n");
    }
    uart_puts("[zenin-status] CPU utilization, power, IPC latency, and FPS: not measured\n");

    /* 14. Live Multi-Device Interactive Graphical OS Session */
    uart_puts("\n=====================================================\n");
    uart_puts("   PROJECT ZENIN: LIVE GRAPHICAL INTERACTIVE SESSION \n");
    uart_puts("=====================================================\n");
    uart_puts("[zenin-shell] RGB565 software framebuffer active; QEMU test dumps over UART\n");
    uart_puts("[zenin-shell] UART controls: keys [1]-[4] select parser-demo surfaces\n");
    uart_puts("[zenin-shell] No native-device input driver or app runtime is active\n");

    compositor_render_frame();
    if (irq_ready && zenin_irq_test_wfi()) {
        uart_puts("[zenin-irq] PASS: one timer IRQ woke the core from WFI.\n");
    } else {
        uart_puts("[zenin-irq] FAIL: timer wake from WFI was not verified.\n");
    }
    fb_dump_frame();

    while (1) {
        if (uart_has_char()) {
            char key = uart_getc();
            if (key == 'I') {
                /* Check for atomic input packet: INP:<16 hex digits> */
                char n1 = uart_getc();
                char n2 = uart_getc();
                char n3 = uart_getc();
                if (n1 == 'N' && n2 == 'P' && n3 == ':') {
                    uint64_t raw64 = 0;
                    for (int i = 0; i < 16; i++) {
                        char hc = uart_getc();
                        raw64 = (raw64 << 4) | hex_to_nibble(hc);
                    }
                    /* Discard trailing newline */
                    while (uart_has_char()) {
                        char term = uart_getc();
                        if (term == '\n' || term == '\r') break;
                    }
                    /* Store the received packet; latency is not measured. */
                    input_inject_packet(raw64);
                    zenin_input_packet_t pkt;
                    pkt.raw64 = raw64;
                    if (pkt.data.device == INPUT_DEV_MOUSE || pkt.data.device == INPUT_DEV_TOUCH) {
                        uint32_t px = (uint32_t)(uint16_t)pkt.data.x_or_key;
                        uint32_t py = (uint32_t)(uint16_t)pkt.data.y_or_axis;
                        if (pkt.data.event == (uint8_t)INPUT_EVT_DOWN) {
                            compositor_handle_touch(px, py, TOUCH_EVENT_DOWN);
                        }
                    }
                    compositor_render_frame();
                    fb_dump_frame();
                    continue;
                }
            }
            if (key == '\r' || key == '\n') {
                continue; /* Ignore stray line endings */
            }

            /* Container Switching: Global Window Manager Controls */
            if (key >= '1' && key <= '4') {
                zenin_container_id_t cid = (zenin_container_id_t)(key - '0');
                uart_puts("[zenin-wm] Switched active container to ID: ");
                uart_put_dec((uint32_t)cid);
                uart_puts("\n");
                compositor_switch_container(cid);
                fb_dump_frame();
            } else if (key == 'd' || key == 'D') {
                uart_puts("[zenin-wm] Manual display scanout refresh\n");
                compositor_render_frame();
                fb_dump_frame();
            } else {
                /* Pure Raw Keyboard Forwarding: NO key interception, NO fake action triggers */
                input_handle_keyboard((uint16_t)key, true);
                compositor_render_frame();
                fb_dump_frame();
                input_handle_keyboard((uint16_t)key, false);
            }
        } else {
            __asm__ volatile("wfi");
        }
    }
}
