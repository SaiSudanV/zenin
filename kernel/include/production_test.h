/*
 * Project Zenin - Production Ingestion & End-to-End Game Testing (Layer 12)
 * Orchestrates multi-OS production games and heavy applications:
 *  1. Android APK: Subway Surfers (DEX + libmain.so + GLES + OpenSL audio)
 *  2. Windows EXE: Cyberpunk 2D (PE32+ + Win32 GDI + Direct3D + XAudio2)
 *  3. Linux ELF:   Minecraft Bedrock Server (POSIX + Z-GL mesh + Z-Bus)
 *  4. macOS APP:   GarageBand (Mach-O 64 + Apple Metal primitives + CoreAudio)
 * Enforces strict metrics: Peak RAM < 120 MB (Budget: 1 GB) | CPU < 30% | 0 ns IPC.
 */

#ifndef ZENIN_PRODUCTION_TEST_H
#define ZENIN_PRODUCTION_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "compat.h"
#include "zpkg.h"
#include "zld.h"
#include "unified_bridge.h"

typedef struct {
    const char *game_name;
    const char *os_origin;
    size_t memory_footprint_kb;
    uint32_t simulated_cpu_load_pct;
    uint32_t active_fps;
    bool is_running;
} zenin_production_game_session_t;

/* Initialize Layer 12 Production Test Engine */
void production_test_init(void);

/* Run complete End-to-End multi-OS production test suite */
bool production_run_all_games(void);

/* Query aggregate production metrics */
void production_get_summary(size_t *out_total_ram_kb, uint32_t *out_avg_cpu_pct, uint32_t *out_min_fps);

#endif /* ZENIN_PRODUCTION_TEST_H */
