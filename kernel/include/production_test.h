/* Kernel smoke checks only. These do not execute games or establish performance. */

#ifndef ZENIN_PRODUCTION_TEST_H
#define ZENIN_PRODUCTION_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define ZENIN_METRIC_UNAVAILABLE UINT32_MAX

/* Reset smoke-check accounting. */
void production_test_init(void);

/* Execute allocator, Z-Bus round-trip, and input-register smoke checks. */
bool production_run_smoke_tests(void);

/* PMM-accounted bytes; CPU and FPS are unavailable (UINT32_MAX). */
void production_get_summary(size_t *out_total_ram_kb, uint32_t *out_avg_cpu_pct, uint32_t *out_min_fps);

#endif /* ZENIN_PRODUCTION_TEST_H */
