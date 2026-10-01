/*
 * Project Zenin - Universal Hardware Abstraction Layer (HAL)
 * Connects directly to Linux Direct Rendering Manager (DRM/KMS),
 * evdev touch input, and libhybris / Android Treble shims.
 */

#ifndef ZENIN_HAL_H
#define ZENIN_HAL_H

#include <stdint.h>
#include <stdbool.h>

/* Display Capabilities */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;
    uint32_t bpp;
    bool is_drm_kms;
} zenin_display_info_t;

/* Battery & Power Capabilities */
typedef struct {
    uint32_t percentage;
    bool is_charging;
    uint32_t current_now_ma;
    uint32_t temp_celsius;
} zenin_power_info_t;

/* HAL Function Prototypes */
int zenin_hal_display_init(zenin_display_info_t *out_info);
int zenin_hal_input_listen(void (*on_touch_event)(int x, int y, int action));
int zenin_hal_power_query(zenin_power_info_t *out_power);

#endif /* ZENIN_HAL_H */
