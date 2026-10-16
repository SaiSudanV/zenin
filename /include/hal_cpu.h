/*
 * Project Zenin - Universal Hardware CPU Intrinsics (Zero-Overhead Interface)
 * Direct static inlined CPU control for:
 *   - AArch64 (ARMv8 Cortex-A53 baseline)
 *   - x86_64 (Standard PC / AMD / Intel)
 * Zero jump table overhead, zero function call overhead.
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

static inline __attribute__((always_inline)) uint32_t hal_get_core_id(void) {
    /* CPUID APIC ID */
    return 0;
}

static inline __attribute__((always_inline)) void hal_memory_barrier(void) {
    __asm__ volatile("mfence" : : : "memory");
}

#else
/* Fallback / Generic Target */
static inline __attribute__((always_inline)) void hal_cpu_idle(void) {}
static inline __attribute__((always_inline)) uint64_t hal_get_cycles(void) { return 0; }
static inline __attribute__((always_inline)) uint32_t hal_get_core_id(void) { return 0; }
static inline __attribute__((always_inline)) void hal_memory_barrier(void) {}
#endif

#endif /* ZENIN_HAL_CPU_H */
