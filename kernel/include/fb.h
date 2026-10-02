/*
 * Project Zenin - Universal Direct Framebuffer (Display Engine)
 * Direct memory-mapped dual-mode (16-bit RGB565 / 32-bit ARGB8888) Framebuffer.
 * In 16-bit RGB565 mode, VRAM footprint is halved to only ~1.84 MB.
 */

#ifndef ZENIN_FB_H
#define ZENIN_FB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FB_COLOR_BLACK_32   0x00000000
#define FB_COLOR_WHITE_32   0x00FFFFFF
#define FB_COLOR_ZENIN_32   0x0000E5FF  /* Zenin Electric Cyan */

#define FB_COLOR_BLACK_16   0x0000
#define FB_COLOR_WHITE_16   0xFFFF
#define FB_COLOR_ZENIN_16   0x073F      /* RGB565 Zenin Cyan */

typedef enum {
    FB_FORMAT_ARGB8888 = 32, /* Crisp 32-bit true color */
    FB_FORMAT_RGB565   = 16  /* Extreme Low-RAM 16-bit mode (Half VRAM) */
} zenin_fb_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    zenin_fb_format_t format;
    void *buffer;
} zenin_framebuffer_t;

/* Initialize framebuffer in either 16-bit or 32-bit mode */
void fb_init_format(uint32_t width, uint32_t height, void *fb_address, zenin_fb_format_t format);

/* Backward compatible 32-bit init */
void fb_init(uint32_t width, uint32_t height, void *fb_address);

/* Clear screen */
void fb_clear(uint32_t color);

/* Draw a pixel */
void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Draw a filled rectangle */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Convert 32-bit ARGB to 16-bit RGB565 */
static inline uint16_t fb_argb_to_rgb565(uint32_t c) {
    uint16_t r = (c >> 19) & 0x1F;
    uint16_t g = (c >> 10) & 0x3F;
    uint16_t b = (c >> 3)  & 0x1F;
    return (r << 11) | (g << 5) | b;
}

/* Draw text */
void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t color, uint32_t scale);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t color, uint32_t scale);

/* Query active display configuration */
const zenin_framebuffer_t *fb_get_info(void);

#endif /* ZENIN_FB_H */
