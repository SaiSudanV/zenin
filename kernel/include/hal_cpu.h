/*
 * Project Zenin CPU primitives.
 * The kernel boot/runtime is currently implemented only for AArch64.
 * The x86_64 block below contains partial instruction primitives; it is not
 * a bootable or verified x86_64 port. Unsupported ISAs fail at compile time
 * instead of silently receiving no-op CPU operations.
 */

#ifndef ZENIN_HAL_CPU_H
#define ZENIN_HAL_CPU_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__aarch64__)
#include "arch/aarch64/cpu.h"

static inline __attribute__((always_inline)) void hal_cpu_idle(void) {
    arch_cpu_wfi();
}

static inline __attribute__((always_inline)) uint64_t hal_get_cycles(void) {
    return arch_get_cycles();
}

static inline __attribute__((always_inline)) uint64_t hal_get_counter_frequency(void) {
    return arch_get_counter_frequency();
}

static inline __attribute__((always_inline)) uint32_t hal_get_core_id(void) {
    return arch_get_core_id();
}

static inline __attribute__((always_inline)) void hal_memory_barrier(void) {
    arch_cpu_dmb();
}

#elif defined(__x86_64__)

static inline __attribute__((always_inline)) void hal_cpu_idle(void) {
    __asm__ volatile("hlt" : : : "memory");
}

static inline __attribute__((always_inline)) uint64_t hal_get_cycles(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline __attribute__((always_inline)) uint64_t hal_get_counter_frequency(void) {
    return 0; /* RDTSC requires platform calibration. */
}

static inline __attribute__((always_inline)) uint32_t hal_get_core_id(void) {
    uint32_t eax = 1, ebx, ecx = 0, edx;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    (void)ecx;
    (void)edx;
    return ebx >> 24;
}

static inline __attribute__((always_inline)) void hal_memory_barrier(void) {
    __asm__ volatile("mfence" : : : "memory");
}

#else
#error "Zenin has no CPU HAL for this processor architecture yet"
#endif

#endif /* ZENIN_HAL_CPU_H */
