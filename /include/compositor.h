/*
 * Project Zenin - Generic Micro-Compositor & Multi-Container Window Manager
 * 
 * Complies strictly with Reality-Bound Engineering (Rule 5: No Fake Implementations):
 * - ZERO game-specific code, headers, or mock graphics.
 * - Pure surface blitter: composites active container backbuffer into physical display.
 * - Displays authentic microkernel hardware telemetry & live atomic input stream.
 */

#ifndef ZENIN_COMPOSITOR_H
#define ZENIN_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/fb.h"
#include "../include/input.h"

/* Generic Container Runtime Identifiers */
typedef enum {
    CONTAINER_ID_NONE    = 0,
    CONTAINER_ID_ANDROID = 1,
    CONTAINER_ID_WINDOWS = 2,
    CONTAINER_ID_LINUX   = 3,
    CONTAINER_ID_MACOS   = 4
} zenin_container_id_t;

/* Generic Container Surface Specification */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint16_t *pixels;             /* Direct 16-bit RGB565 surface buffer */
    bool is_active;
    uint32_t frames_rendered;     /* Incremented ONLY when a real draw call presents */
    const char *runtime_name;     /* "Android Runtime", "Win32 / NT Subsystem", etc. */
    const char *abi_type;         /* "Bionic / ELF64", "Win32 PE32+ / x86_64", etc. */
    const char *execution_state;  /* Current execution status */
} zenin_container_surface_t;

/* Initialize compositor and register generic container surfaces */
void compositor_init(uint32_t width, uint32_t height);

/* Container Management */
void compositor_switch_container(zenin_container_id_t id);
zenin_container_id_t compositor_get_active_container(void);
zenin_container_surface_t *compositor_get_surface(zenin_container_id_t id);

/* Update container presentation state */
void compositor_post_surface_frame(zenin_container_id_t id);

/* Render complete display scanout frame */
void compositor_render_frame(void);

/* Touch navigation for container tabs */
void compositor_handle_touch(uint32_t x, uint32_t y, zenin_touch_type_t type);

#endif /* ZENIN_COMPOSITOR_H */
