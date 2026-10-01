/*
 * Project Zenin - Hardware Touch Digitizer & Pointer Input Driver
 * Converts hardware touch/pointer interrupts into zero-copy Z-Bus events.
 */

#ifndef ZENIN_INPUT_H
#define ZENIN_INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOUCH_EVENT_NONE = 0,
    TOUCH_EVENT_DOWN,
    TOUCH_EVENT_UP,
    TOUCH_EVENT_MOVE
} zenin_touch_type_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    zenin_touch_type_t type;
    uint32_t pressure;
} zenin_touch_point_t;

/* Initialize hardware touch digitizer */
void input_init(uint32_t max_x, uint32_t max_y);

/* Handle raw hardware touch interrupt and dispatch to Z-Bus */
void input_handle_hardware_touch(uint32_t x, uint32_t y, zenin_touch_type_t type);

/* Query last known touch position */
const zenin_touch_point_t *input_get_last_touch(void);

#endif /* ZENIN_INPUT_H */
