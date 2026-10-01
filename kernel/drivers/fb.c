/*
 * Project Zenin - Universal Direct Framebuffer Implementation
 * Direct hardware framebuffer rendering.
 * Zero-copy memory scanout for 60/120 FPS gaming and fluid UI.
 */

#include "../include/fb.h"

static zenin_framebuffer_t fb_device;

void fb_init(uint32_t width, uint32_t height, void *fb_address) {
    fb_device.width = width;
    fb_device.height = height;
    fb_device.pitch = width * sizeof(uint32_t);
    fb_device.bpp = 32;
    fb_device.buffer = (uint32_t *)fb_address;
}

void fb_clear(uint32_t color) {
    if (!fb_device.buffer) return;
    size_t total_pixels = fb_device.width * fb_device.height;
    for (size_t i = 0; i < total_pixels; i++) {
        fb_device.buffer[i] = color;
    }
}

void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_device.buffer || x >= fb_device.width || y >= fb_device.height) {
        return;
    }
    fb_device.buffer[y * fb_device.width + x] = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_device.buffer) return;

    uint32_t x_end = (x + w > fb_device.width) ? fb_device.width : (x + w);
    uint32_t y_end = (y + h > fb_device.height) ? fb_device.height : (y + h);

    for (uint32_t cy = y; cy < y_end; cy++) {
        uint32_t row_offset = cy * fb_device.width;
        for (uint32_t cx = x; cx < x_end; cx++) {
            fb_device.buffer[row_offset + cx] = color;
        }
    }
}

const zenin_framebuffer_t *fb_get_info(void) {
    return &fb_device;
}
