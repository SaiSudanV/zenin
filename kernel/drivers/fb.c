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

static const uint8_t font5x7[128][5] = {
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['+'] = {0x08, 0x08, 0x3E, 0x08, 0x08},
    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
    ['/'] = {0x20, 0x10, 0x08, 0x04, 0x02},
    ['%'] = {0x23, 0x13, 0x08, 0x64, 0x62},
    ['['] = {0x00, 0x7F, 0x41, 0x41, 0x00},
    [']'] = {0x00, 0x41, 0x41, 0x7F, 0x00},
    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},
    ['A'] = {0x7C, 0x12, 0x11, 0x12, 0x7C},
    ['B'] = {0x7F, 0x49, 0x49, 0x49, 0x36},
    ['C'] = {0x3E, 0x41, 0x41, 0x41, 0x22},
    ['D'] = {0x7F, 0x41, 0x41, 0x22, 0x1C},
    ['E'] = {0x7F, 0x49, 0x49, 0x49, 0x41},
    ['F'] = {0x7F, 0x09, 0x09, 0x09, 0x01},
    ['G'] = {0x3E, 0x41, 0x49, 0x49, 0x7A},
    ['H'] = {0x7F, 0x08, 0x08, 0x08, 0x7F},
    ['I'] = {0x00, 0x41, 0x7F, 0x41, 0x00},
    ['J'] = {0x20, 0x40, 0x41, 0x3F, 0x01},
    ['K'] = {0x7F, 0x08, 0x14, 0x22, 0x41},
    ['L'] = {0x7F, 0x40, 0x40, 0x40, 0x40},
    ['M'] = {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    ['N'] = {0x7F, 0x04, 0x08, 0x10, 0x7F},
    ['O'] = {0x3E, 0x41, 0x41, 0x41, 0x3E},
    ['P'] = {0x7F, 0x09, 0x09, 0x09, 0x06},
    ['Q'] = {0x3E, 0x41, 0x51, 0x21, 0x5E},
    ['R'] = {0x7F, 0x09, 0x19, 0x29, 0x46},
    ['S'] = {0x46, 0x49, 0x49, 0x49, 0x31},
    ['T'] = {0x01, 0x01, 0x7F, 0x01, 0x01},
    ['U'] = {0x3F, 0x40, 0x40, 0x40, 0x3F},
    ['V'] = {0x1F, 0x20, 0x40, 0x20, 0x1F},
    ['W'] = {0x7F, 0x20, 0x18, 0x20, 0x7F},
    ['X'] = {0x63, 0x14, 0x08, 0x14, 0x63},
    ['Y'] = {0x07, 0x08, 0x70, 0x08, 0x07},
    ['Z'] = {0x61, 0x51, 0x49, 0x45, 0x43}
};

void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale) {
    if (scale == 0) scale = 1;
    uint8_t uc = (uint8_t)c;
    if (uc >= 'a' && uc <= 'z') uc -= 32; /* Convert to uppercase for 5x7 font */
    if (uc >= 128) return;

    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[uc][col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                fb_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t color, uint32_t scale) {
    if (!str) return;
    uint32_t cur_x = x;
    for (size_t i = 0; str[i] != '\0'; i++) {
        fb_draw_char(cur_x, y, str[i], color, scale);
        cur_x += 6 * scale;
    }
}

const zenin_framebuffer_t *fb_get_info(void) {
    return &fb_device;
}

