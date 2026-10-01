/*
 * Project Zenin - Universal Direct Framebuffer (Display Engine)
 * Direct memory-mapped 32-bit ARGB framebuffer.
 * Hardware DMA scanout for 60/120 FPS zero-lag display output.
 */

#ifndef ZENIN_FB_H
#define ZENIN_FB_H

#include <stdint.h>
#include <stddef.h>

#define FB_COLOR_BLACK   0x00000000
#define FB_COLOR_WHITE   0x00FFFFFF
#define FB_COLOR_CYAN    0x0000FFFF
#define FB_COLOR_ZENIN   0x0000E5FF  /* Zenin Electric Cyan */

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t *buffer;
} zenin_framebuffer_t;

/* Initialize framebuffer at hardware memory address */
void fb_init(uint32_t width, uint32_t height, void *fb_address);

/* Clear screen to a solid color */
void fb_clear(uint32_t color);

/* Draw a single 32-bit ARGB pixel */
void fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Draw a filled rectangle */
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/* Query active display configuration */
const zenin_framebuffer_t *fb_get_info(void);

#endif /* ZENIN_FB_H */
