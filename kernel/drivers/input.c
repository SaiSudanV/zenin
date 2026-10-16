/*
 * Project Zenin - UART-fed input packet state for the current QEMU prototype.
 */

#include "../include/input.h"
#include "../include/zbus.h"
#include "../include/uart.h"

/* Atomic hardware input register cache (volatile single 64-bit word) */
static volatile uint64_t g_atomic_input_state = 0;
static zenin_touch_point_t last_touch;
static uint32_t screen_max_x = 360;
static uint32_t screen_max_y = 640;

void input_init(uint32_t max_x, uint32_t max_y) {
    screen_max_x = max_x;
    screen_max_y = max_y;
    g_atomic_input_state = 0;
    last_touch.x = 0;
    last_touch.y = 0;
    last_touch.type = TOUCH_EVENT_NONE;
    last_touch.pressure = 0;
    uart_puts("[zenin-input] Input packet state initialized; hardware input drivers are not present.\n");
}

/* Store the packet in the kernel's current input state. Latency is unmeasured. */
void input_inject_packet(uint64_t raw64_packet) {
    g_atomic_input_state = raw64_packet;

    /* Dispatch atomic 64-bit pointer/value directly to Z-Bus ring */
    zbus_publish(0x04, 0x20, ZBUS_MSG_TOUCH_EVT, &raw64_packet, sizeof(uint64_t));
}

void input_handle_mouse(int16_t x, int16_t y, uint16_t buttons, zenin_input_event_type_t event) {
    zenin_input_packet_t pkt;
    pkt.data.device = (uint8_t)INPUT_DEV_MOUSE;
    pkt.data.event = (uint8_t)event;
    pkt.data.x_or_key = x;
    pkt.data.y_or_axis = y;
    pkt.data.buttons = buttons;
    input_inject_packet(pkt.raw64);
}

void input_handle_keyboard(uint16_t keycode, bool is_down) {
    zenin_input_packet_t pkt;
    pkt.data.device = (uint8_t)INPUT_DEV_KEYBOARD;
    pkt.data.event = is_down ? (uint8_t)INPUT_EVT_DOWN : (uint8_t)INPUT_EVT_UP;
    pkt.data.x_or_key = (int16_t)keycode;
    pkt.data.y_or_axis = 0;
    pkt.data.buttons = 0;
    input_inject_packet(pkt.raw64);
}

void input_handle_gamepad(int8_t stick_lx, int8_t stick_ly, int8_t stick_rx, int8_t stick_ry, uint16_t buttons) {
    zenin_input_packet_t pkt;
    pkt.data.device = (uint8_t)INPUT_DEV_GAMEPAD;
    pkt.data.event = (uint8_t)INPUT_EVT_AXIS;
    /* Pack stick LX/LY into x_or_key, RX/RY into y_or_axis */
    pkt.data.x_or_key = (int16_t)(((uint16_t)(uint8_t)stick_lx) | (((uint16_t)(uint8_t)stick_ly) << 8));
    pkt.data.y_or_axis = (int16_t)(((uint16_t)(uint8_t)stick_rx) | (((uint16_t)(uint8_t)stick_ry) << 8));
    pkt.data.buttons = buttons;
    input_inject_packet(pkt.raw64);
}

void input_handle_hardware_touch(uint32_t x, uint32_t y, zenin_touch_type_t type) {
    if (x > screen_max_x) x = screen_max_x;
    if (y > screen_max_y) y = screen_max_y;

    last_touch.x = x;
    last_touch.y = y;
    last_touch.type = type;
    last_touch.pressure = (type == TOUCH_EVENT_UP) ? 0 : 255;

    zenin_input_packet_t pkt;
    pkt.data.device = (uint8_t)INPUT_DEV_TOUCH;
    pkt.data.event = (uint8_t)type;
    pkt.data.x_or_key = (int16_t)x;
    pkt.data.y_or_axis = (int16_t)y;
    pkt.data.buttons = (type == TOUCH_EVENT_UP) ? 0 : 1;
    input_inject_packet(pkt.raw64);
}

zenin_input_packet_t input_get_current_state(void) {
    zenin_input_packet_t pkt;
    pkt.raw64 = g_atomic_input_state;
    return pkt;
}

const zenin_touch_point_t *input_get_last_touch(void) {
    return &last_touch;
}
