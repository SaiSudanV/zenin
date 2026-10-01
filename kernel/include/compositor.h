/*
 * Project Zenin - Micro-Compositor & Interactive Shell Renderer
 * Paints Status Bar, Interactive Buttons, and Live Performance Meters directly to the Framebuffer.
 */

#ifndef ZENIN_COMPOSITOR_H
#define ZENIN_COMPOSITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "../include/fb.h"
#include "../include/input.h"

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    const char *label;
    uint32_t color;
    bool is_pressed;
} zenin_button_t;

/* Initialize compositor layout */
void compositor_init(uint32_t width, uint32_t height);

/* Render complete desktop shell frame */
void compositor_render_frame(void);

/* Process touch input and update interactive button states */
void compositor_handle_touch(uint32_t x, uint32_t y, zenin_touch_type_t type);

#endif /* ZENIN_COMPOSITOR_H */
