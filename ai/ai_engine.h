/*
 * Project Zenin - AI Intent Dispatcher
 * Directly bridges natural language/contextual AI outputs to hardware/OS actions
 * Eliminates Android's 80+ bloated Java Manager services
 */

#ifndef ZENIN_AI_ENGINE_H
#define ZENIN_AI_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

/* Standard AI Hardware Intents */
typedef enum {
    INTENT_UNKNOWN = 0,
    INTENT_TOGGLE_TORCH,
    INTENT_SET_BRIGHTNESS,
    INTENT_SET_VOLUME,
    INTENT_CONNECT_WIFI,
    INTENT_TOGGLE_AIRPLANE_MODE,
    INTENT_LAUNCH_APP,
    INTENT_QUERY_BATTERY,
    INTENT_SUMMARIZE_SCREEN
} zenin_intent_type_t;

typedef struct {
    zenin_intent_type_t type;
    int32_t value_int;
    char target_str[128];
    float confidence;
} zenin_intent_t;

/* Parse raw natural language query into concrete OS hardware intent */
int zenin_ai_resolve_intent(const char *prompt, zenin_intent_t *out_intent);

/* Execute resolved intent directly on hardware via HAL/Z-Bus */
int zenin_ai_dispatch_intent(const zenin_intent_t *intent);

#endif /* ZENIN_AI_ENGINE_H */
