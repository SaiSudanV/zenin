/*
 * Project Zenin - Micro-Compositor & Interactive Shell Renderer Implementation
 * Directly draws the Status Bar, Launcher Buttons, and Live Performance Meters.
 */

#include "../include/compositor.h"
#include "../include/fb.h"
#include "../include/uart.h"

#define COLOR_BG          0x000F172A  /* Dark Slate Navy */
#define COLOR_STATUS_BAR  0x001E293B  /* Slate Blue */
#define COLOR_ACCENT      0x0000E5FF  /* Electric Cyan */
#define COLOR_PRESSED     0x0038BDF8  /* Sky Blue */
#define COLOR_TEXT        0x00F8FAFC  /* Crisp White */

static zenin_button_t btn_game_mode = { 60, 200, 240, 80, "TURBO GAME", COLOR_ACCENT, false };
static zenin_button_t btn_ai_voice  = { 60, 320, 240, 80, "ZENIN AI",   0x00A855F7,   false };

void compositor_init(uint32_t width, uint32_t height) {
    (void)width;
    (void)height;
}

void compositor_render_frame(void) {
    /* 1. Clear background to smooth dark slate */
    fb_clear(COLOR_BG);

    /* 2. Render Status Bar (Height: 48px) */
    fb_fill_rect(0, 0, 720, 48, COLOR_STATUS_BAR);
    /* Battery pill icon indicator */
    fb_fill_rect(650, 14, 40, 20, COLOR_ACCENT);

    /* 3. Render Interactive Touch Buttons */
    uint32_t c1 = btn_game_mode.is_pressed ? COLOR_PRESSED : btn_game_mode.color;
    fb_fill_rect(btn_game_mode.x, btn_game_mode.y, btn_game_mode.w, btn_game_mode.h, c1);

    uint32_t c2 = btn_ai_voice.is_pressed ? COLOR_PRESSED : btn_ai_voice.color;
    fb_fill_rect(btn_ai_voice.x, btn_ai_voice.y, btn_ai_voice.w, btn_ai_voice.h, c2);

    /* 4. Render Live Performance Monitor Bar (Simulating 18% CPU load) */
    fb_fill_rect(60, 500, 600, 30, COLOR_STATUS_BAR);
    fb_fill_rect(60, 500, (600 * 18) / 100, 30, COLOR_ACCENT); /* Active Load Gauge */
}

void compositor_handle_touch(uint32_t x, uint32_t y, zenin_touch_type_t type) {
    if (type == TOUCH_EVENT_DOWN) {
        if (x >= btn_game_mode.x && x <= btn_game_mode.x + btn_game_mode.w &&
            y >= btn_game_mode.y && y <= btn_game_mode.y + btn_game_mode.h) {
            btn_game_mode.is_pressed = true;
            uart_puts("[compositor] Touch Tap: TURBO GAME MODE Button Pressed!\n");
        }
        if (x >= btn_ai_voice.x && x <= btn_ai_voice.x + btn_ai_voice.w &&
            y >= btn_ai_voice.y && y <= btn_ai_voice.y + btn_ai_voice.h) {
            btn_ai_voice.is_pressed = true;
            uart_puts("[compositor] Touch Tap: ZENIN AI Voice Button Pressed!\n");
        }
    } else if (type == TOUCH_EVENT_UP) {
        btn_game_mode.is_pressed = false;
        btn_ai_voice.is_pressed = false;
        uart_puts("[compositor] Touch Release: Interactive Button States Reset.\n");
    }

    /* Redraw frame with updated state */
    compositor_render_frame();
}
