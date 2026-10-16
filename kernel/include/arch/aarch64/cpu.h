/*
 * Project Zenin - Hardware CPU Intrinsics & Direct Execution Primitives
 * Architecture: AArch64 (ARMv8-A Cortex-A53 baseline)
 * Fully inlined intrinsics: no wrapper function call; hardware instruction
 * latency remains nonzero and must be measured on the target.
 */

#ifndef ZENIN_ARCH_AARCH64_CPU_H
#define ZENIN_ARCH_AARCH64_CPU_H

#include <stdint.h>

/* Enter low-power standby until hardware interrupt fires (0% CPU idle duty) */
static inline __attribute__((always_inline)) void arch_cpu_wfi(void) {
    __asm__ volatile("wfi" : : : "memory");
}

/* Data Memory Barrier */
static inline __attribute__((always_inline)) void arch_cpu_dmb(void) {
    __asm__ volatile("dmb sy" : : : "memory");
}

/* Data Synchronization Barrier */
static inline __attribute__((always_inline)) void arch_cpu_dsb(void) {
    __asm__ volatile("dsb sy" : : : "memory");
}

/* Instruction Synchronization Barrier */
static inline __attribute__((always_inline)) void arch_cpu_isb(void) {
    __asm__ volatile("isb" : : : "memory");
}

/* Read the architectural virtual counter (counter ticks, not nanoseconds). */
static inline __attribute__((always_inline)) uint64_t arch_get_cycles(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

/* Architectural generic timer frequency in ticks per second. */
static inline __attribute__((always_inline)) uint64_t arch_get_counter_frequency(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/* Read CPU Core Hardware Thread ID (0 or 1 for dual-core) */
static inline __attribute__((always_inline)) uint32_t arch_get_core_id(void) {
    uint64_t mpidr;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return (uint32_t)(mpidr & 0xFF);
}

/* Direct MMIO 32-bit register read/write (single STR/LDR instruction) */
#define MMIO_WRITE32(addr, val) (*(volatile uint32_t *)(uintptr_t)(addr) = (uint32_t)(val))
#define MMIO_READ32(addr)        (*(volatile uint32_t *)(uintptr_t)(addr))

#define MMIO_WRITE64(addr, val) (*(volatile uint64_t *)(uintptr_t)(addr) = (uint64_t)(val))
#define MMIO_READ64(addr)        (*(volatile uint64_t *)(uintptr_t)(addr))

#endif /* ZENIN_ARCH_AARCH64_CPU_H */
