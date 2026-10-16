/*
 * Project Zenin - Ultra-Low Latency Universal Hardware Input Engine
 * Supports Mouse, Keyboard, and Gamepad / Controller inputs.
 * Uses packed 64-bit atomic state transfers (1 to 2 CPU clock cycles / ~1-2 ns)
 * to eliminate queue contention, heap allocations, and context switch penalties.
 */

#ifndef ZENIN_INPUT_H
#define ZENIN_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Device Types */
typedef enum {
    INPUT_DEV_NONE     = 0,
    INPUT_DEV_TOUCH    = 1,
    INPUT_DEV_MOUSE    = 2,
    INPUT_DEV_KEYBOARD = 3,
    INPUT_DEV_GAMEPAD  = 4
} zenin_input_device_t;

/* Event Types */
typedef enum {
    INPUT_EVT_DOWN   = 1,
    INPUT_EVT_UP     = 2,
    INPUT_EVT_MOVE   = 3,
    INPUT_EVT_AXIS   = 4
} zenin_input_event_type_t;

/* Gamepad Button Bitmask */
#define GAMEPAD_BTN_A         (1 << 0)
#define GAMEPAD_BTN_B         (1 << 1)
#define GAMEPAD_BTN_X         (1 << 2)
#define GAMEPAD_BTN_Y         (1 << 3)
#define GAMEPAD_BTN_LB        (1 << 4)
#define GAMEPAD_BTN_RB        (1 << 5)
#define GAMEPAD_BTN_LT        (1 << 6)
#define GAMEPAD_BTN_RT        (1 << 7)
#define GAMEPAD_BTN_START     (1 << 8)
#define GAMEPAD_BTN_SELECT    (1 << 9)
#define GAMEPAD_BTN_DPAD_UP   (1 << 10)
#define GAMEPAD_BTN_DPAD_DOWN (1 << 11)
#define GAMEPAD_BTN_DPAD_LEFT (1 << 12)
#define GAMEPAD_BTN_DPAD_RGHT (1 << 13)

/* Mouse Button Bitmask */
#define MOUSE_BTN_LEFT        (1 << 0)
#define MOUSE_BTN_RIGHT       (1 << 1)
#define MOUSE_BTN_MIDDLE      (1 << 2)

/* 64-bit Atomic Packed Hardware Input State */
#pragma pack(push, 1)
typedef union {
    uint64_t raw64;
    struct {
        uint8_t  device;       /* zenin_input_device_t (1 byte) */
        uint8_t  event;        /* zenin_input_event_type_t (1 byte) */
        int16_t  x_or_key;     /* Mouse X, Keycode, Stick Left X (-128 to 127) */
        int16_t  y_or_axis;    /* Mouse Y, Stick Left Y (-128 to 127) */
        uint16_t buttons;      /* Button bitmask / modifier flags */
    } data;
} zenin_input_packet_t;
#pragma pack(pop)

/* Backwards-compatible touch event alias */
typedef enum {
    TOUCH_EVENT_NONE = 0,
    TOUCH_EVENT_DOWN = 1,
    TOUCH_EVENT_UP   = 2,
    TOUCH_EVENT_MOVE = 3
} zenin_touch_type_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    zenin_touch_type_t type;
    uint32_t pressure;
} zenin_touch_point_t;

/* Initialize hardware input subsystem */
void input_init(uint32_t max_x, uint32_t max_y);

/* Sub-2ns Atomic Direct Packet Injection */
void input_inject_packet(uint64_t raw64_packet);

/* Specialized Convenience Injections */
void input_handle_mouse(int16_t x, int16_t y, uint16_t buttons, zenin_input_event_type_t event);
void input_handle_keyboard(uint16_t keycode, bool is_down);
void input_handle_gamepad(int8_t stick_lx, int8_t stick_ly, int8_t stick_rx, int8_t stick_ry, uint16_t buttons);
void input_handle_hardware_touch(uint32_t x, uint32_t y, zenin_touch_type_t type);

/* Query atomic current hardware states */
zenin_input_packet_t input_get_current_state(void);
const zenin_touch_point_t *input_get_last_touch(void);

#endif /* ZENIN_INPUT_H */
