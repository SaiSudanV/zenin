/*
 * Project Zenin - Bare-Metal ARM64 Bootstrapping
 * Architecture: AArch64 (ARMv8-A / ARMv9-A)
 */

.section ".text.boot"
.global _start

_start:
    /* 1. Only allow Core 0 to initialize the kernel. Park secondary cores for now. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF       // Extract Core ID (affinity 0)
    cbz     x0, master_core

park_core:
    /* Put secondary cores into low-power WFE sleep */
    wfe
    b       park_core

master_core:
    /* 2. Setup Initial Kernel Stack Pointer */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* 3. Zero out the BSS section */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
zero_bss:
    cmp     x1, x2
    b.hs    call_kernel
    str     xzr, [x1], #8
    b       zero_bss

call_kernel:
    /* 4. Jump to C Kernel Main */
    bl      kmain

halt:
    /* 5. Safe power-down halt if kmain returns */
    wfi
    b       halt
