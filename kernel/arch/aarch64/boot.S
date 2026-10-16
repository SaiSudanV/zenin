/*
 * Project Zenin - Bare-Metal ARM64 Bootstrapping
 * Architecture: AArch64 (ARMv8-A / ARMv9-A)
 */

.section ".text.boot"
.global _start

_start:
    /* 0. Preserve the EFI->kernel boot-info pointer (x0) across setup; the
     *    UEFI stub passes the discovered memory map here (Stream C). x19 is
     *    callee-saved so arch_mmu_enable preserves it. */
    mov     x19, x0

    /* 1. Only allow Core 0 to initialize the kernel. Park secondary cores for now. */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF       // Extract Core ID (affinity 0)
    cbz     x0, master_core

park_core:
    /* Put secondary cores into low-power WFE sleep */
    wfe
    b       park_core

master_core:
    /* Enable FP/SIMD (NEON) access in CPACR_EL1 (bits [21:20] = 0b11) */
    mrs     x0, cpacr_el1
    orr     x0, x0, #(0x3 << 20)
    msr     cpacr_el1, x0
    isb

    /* 2. Setup Initial Kernel Stack Pointer */
    ldr     x0, =__stack_top
    mov     sp, x0

    /* 3. Initialized state is pre-baked in the image (D011 A6):
     *    -fno-zero-initialized-in-bss leaves no runtime .bss zeroing. */
    /* Enter the memory-map-driven identity-mapped RAM/MMIO regime. */
    mov     x0, x19
    bl      arch_mmu_enable
    /* 4. Jump to C Kernel Main (x0 = boot-info pointer from the UEFI stub) */
    mov     x0, x19
    bl      kmain

halt:
    /* 5. Safe power-down halt if kmain returns */
    wfi
    b       halt
