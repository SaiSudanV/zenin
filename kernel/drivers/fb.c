/*
 * Project Zenin - Universal Direct Framebuffer Implementation
 * Supports 16-bit RGB565 (extreme low-memory) & 32-bit ARGB8888 (standard).
 */

#include "../include/fb.h"

static zenin_framebuffer_t fb_device;

void fb_init_format(uint32_t width, uint32_t height, void *fb_address, zenin_fb_format_t format) {
    fb_device.width = width;
    fb_device.height = height;
    fb_device.format = format;
    fb_device.pitch = width * (format / 8);
    fb_device.buffer = fb_address;
}

void fb_init(uint32_t width, uint32_t height, void *fb_address) {
    fb_init_format(width, height, fb_address, FB_FORMAT_RGB565); /* Default to 16-bit RGB565 for potato hardware */
}

void fb_clear(uint32_t color) {
    if (!fb_device.buffer) return;
    size_t total_pixels = fb_device.width * fb_device.height;

    if (fb_device.format == FB_FORMAT_RGB565) {
        uint16_t *buf16 = (uint16_t *)fb_device.buffer;
        uint16_t c16 = (color > 0xFFFF) ? fb_argb_to_rgb565(color) : (uint16_t)color;
        for (size_t i = 0; i < total_pixels; i++) {
            buf16[i] = c16;
        }
    } else {
        uint32_t *buf32 = (uint32_t *)fb_device.buffer;
        for (size_t i = 0; i < total_pixels; i++) {
            buf32[i] = color;
        }
    }
}

void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_device.buffer || x >= fb_device.width || y >= fb_device.height) return;

    if (fb_device.format == FB_FORMAT_RGB565) {
        uint16_t *buf16 = (uint16_t *)fb_device.buffer;
        buf16[y * fb_device.width + x] = (color > 0xFFFF) ? fb_argb_to_rgb565(color) : (uint16_t)color;
    } else {
        uint32_t *buf32 = (uint32_t *)fb_device.buffer;
        buf32[y * fb_device.width + x] = color;
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb_device.buffer) return;

    uint32_t x_end = (x + w > fb_device.width) ? fb_device.width : (x + w);
    uint32_t y_end = (y + h > fb_device.height) ? fb_device.height : (y + h);

    if (fb_device.format == FB_FORMAT_RGB565) {
        uint16_t *buf16 = (uint16_t *)fb_device.buffer;
        uint16_t c16 = (color > 0xFFFF) ? fb_argb_to_rgb565(color) : (uint16_t)color;
        for (uint32_t cy = y; cy < y_end; cy++) {
            uint32_t row_offset = cy * fb_device.width;
            for (uint32_t cx = x; cx < x_end; cx++) {
                buf16[row_offset + cx] = c16;
            }
        }
    } else {
        uint32_t *buf32 = (uint32_t *)fb_device.buffer;
        for (uint32_t cy = y; cy < y_end; cy++) {
            uint32_t row_offset = cy * fb_device.width;
            for (uint32_t cx = x; cx < x_end; cx++) {
                buf32[row_offset + cx] = color;
            }
        }
    }
}

const zenin_framebuffer_t *fb_get_info(void) {
    return &fb_device;
}
