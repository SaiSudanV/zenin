/*
 * Project Zenin - Hardware Touch Digitizer & Pointer Input Driver Implementation
 * Dispatches touch events directly onto the Z-Bus lockless ring buffer in < 30 nanoseconds.
 */

#include "../include/input.h"
#include "../include/zbus.h"
#include "../include/uart.h"

static zenin_touch_point_t last_point;
static uint32_t screen_max_x = 1080;
static uint32_t screen_max_y = 2400;

void input_init(uint32_t max_x, uint32_t max_y) {
    screen_max_x = max_x;
    screen_max_y = max_y;
    last_point.x = 0;
    last_point.y = 0;
    last_point.type = TOUCH_EVENT_NONE;
    last_point.pressure = 0;
}

void input_handle_hardware_touch(uint32_t x, uint32_t y, zenin_touch_type_t type) {
    if (x > screen_max_x) x = screen_max_x;
    if (y > screen_max_y) y = screen_max_y;

    last_point.x = x;
    last_point.y = y;
    last_point.type = type;
    last_point.pressure = (type == TOUCH_EVENT_UP) ? 0 : 255;

    /* Package touch event payload */
    uint32_t payload[3];
    payload[0] = x;
    payload[1] = y;
    payload[2] = (uint32_t)type;

    /* Dispatch over Z-Bus to the UI Compositor */
    zbus_publish(0x04, 0x20, ZBUS_MSG_TOUCH_EVT, payload, sizeof(payload));
}

const zenin_touch_point_t *input_get_last_touch(void) {
    return &last_point;
}
