/*
 * Project Zenin - Real-Time Performance Profiler & Application Benchmark Suite
 * Measures exact CPU clock cycles, RAM consumption, render latency, and memory reclamation.
 * Enforces the strict rule: Zero CPU/GPU load for idle OS processes.
 */

#ifndef ZENIN_BENCHMARK_H
#define ZENIN_BENCHMARK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t start_cycles;
    uint64_t end_cycles;
    uint64_t total_cycles;
    size_t ram_before_bytes;
    size_t ram_after_bytes;
    size_t ram_peak_bytes;
} zenin_perf_metric_t;

/* Start real-time hardware cycle profiling */
void perf_start(zenin_perf_metric_t *metric);

/* Stop profiling and calculate elapsed CPU cycles & RAM delta */
void perf_stop(zenin_perf_metric_t *metric);

/* Execute full end-to-end benchmark suite across all OS workloads */
void run_system_performance_benchmark(void);

#endif /* ZENIN_BENCHMARK_H */
