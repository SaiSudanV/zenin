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

typedef enum {
    APP_NONE = 0,
    APP_ANDROID_SUBWAY = 1,
    APP_WINDOWS_CYBERPUNK = 2,
    APP_LINUX_MINECRAFT = 3,
    APP_MACOS_GARAGEBAND = 4
} zenin_active_app_t;

/* Initialize compositor layout */
void compositor_init(uint32_t width, uint32_t height);

/* Render complete desktop shell frame */
void compositor_render_frame(void);

/* Process touch input and update interactive button states */
void compositor_handle_touch(uint32_t x, uint32_t y, zenin_touch_type_t type);

/* Switch active container and render live view */
void compositor_switch_app(zenin_active_app_t app);
zenin_active_app_t compositor_get_active_app(void);


#endif /* ZENIN_COMPOSITOR_H */
