/*
 * Project Zenin - Native AI Context Aggregator & Intent Dispatcher
 * Sub-microsecond pattern matching and hardware state reflection.
 * Translates natural language and contextual sensor states into direct OS actions.
 */

#ifndef ZENIN_AI_CORE_H
#define ZENIN_AI_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    AI_ACTION_NONE = 0,
    AI_ACTION_TURBO_GAME_MODE,
    AI_ACTION_ZERO_DRAIN_SLEEP,
    AI_ACTION_SET_BRIGHTNESS,
    AI_ACTION_LAUNCH_WINDOWS_CONTAINER,
    AI_ACTION_LAUNCH_ANDROID_CONTAINER,
    AI_ACTION_SUMMARIZE_SCREEN
} zenin_ai_action_t;

typedef struct {
    uint32_t battery_percent;
    bool is_charging;
    uint32_t free_ram_mb;
    uint32_t cpu_clock_mhz;
    const char *active_app_title;
} zenin_system_context_t;

/* Initialize AI intent engine */
void zenin_ai_init(void);

/* Query live real-time hardware context */
void zenin_ai_get_context(zenin_system_context_t *out_ctx);

/* Parse natural language or system trigger into executable hardware action */
zenin_ai_action_t zenin_ai_resolve_intent(const char *prompt_or_trigger);

/* Dispatch action directly over Z-Bus to hardware */
bool zenin_ai_dispatch_action(zenin_ai_action_t action);

#endif /* ZENIN_AI_CORE_H */
