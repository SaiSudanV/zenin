/*
 * Project Zenin - Native AI Context Aggregator & Intent Dispatcher Implementation
 * Provides keyword intent prototypes and reports only context fields with
 * implemented data sources. This is not a natural-language model or a
 * hardware action executor.
 */

#include "../include/ai_core.h"
#include "../include/zbus.h"
#include "../include/pmm.h"
#include "../include/compositor.h"

/* Fast string prefix and exact match helpers for freestanding C */
static bool str_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    for (size_t i = 0; haystack[i] != '\0'; i++) {
        size_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') return true;
    }
    return false;
}

void zenin_ai_init(void) {
    /* Ready the AI context cache */
}

void zenin_ai_get_context(zenin_system_context_t *out_ctx) {
    if (!out_ctx) return;
    out_ctx->valid_fields = ZENIN_CONTEXT_MEMORY_VALID | ZENIN_CONTEXT_ACTIVE_SURFACE_VALID;
    out_ctx->battery_percent = 0;
    out_ctx->is_charging = false;
    out_ctx->free_ram_mb = (pmm_get_free_pages() * 4096) / (1024 * 1024);
    out_ctx->total_ram_mb = (pmm_get_total_pages() * 4096) / (1024 * 1024);
    out_ctx->cpu_clock_mhz = 0;
    zenin_container_id_t active = compositor_get_active_container();
    zenin_container_surface_t *surface = compositor_get_surface(active);
    out_ctx->active_container_id = (uint32_t)active;
    out_ctx->active_surface_name = surface ? surface->runtime_name : NULL;
    out_ctx->network_status = NULL;
}

zenin_ai_action_t zenin_ai_resolve_intent(const char *prompt_or_trigger) {
    if (!prompt_or_trigger) return AI_ACTION_NONE;

    if (str_contains(prompt_or_trigger, "game") || str_contains(prompt_or_trigger, "turbo")) {
        return AI_ACTION_TURBO_GAME_MODE;
    }
    if (str_contains(prompt_or_trigger, "sleep") || str_contains(prompt_or_trigger, "battery")) {
        return AI_ACTION_ZERO_DRAIN_SLEEP;
    }
    if (str_contains(prompt_or_trigger, "windows") || str_contains(prompt_or_trigger, ".exe")) {
        return AI_ACTION_LAUNCH_WINDOWS_CONTAINER;
    }
    if (str_contains(prompt_or_trigger, "android") || str_contains(prompt_or_trigger, ".apk")) {
        return AI_ACTION_LAUNCH_ANDROID_CONTAINER;
    }
    if (str_contains(prompt_or_trigger, "summarize") || str_contains(prompt_or_trigger, "screen")) {
        return AI_ACTION_SUMMARIZE_SCREEN;
    }

    return AI_ACTION_NONE;
}

bool zenin_ai_dispatch_action(zenin_ai_action_t action) {
    switch (action) {
        case AI_ACTION_TURBO_GAME_MODE:
            return zbus_publish(0x10, 0x01, ZBUS_MSG_AI_INTENT, "TURBO_GAME_MODE_ACTIVE", 22);
        case AI_ACTION_ZERO_DRAIN_SLEEP:
            return zbus_publish(0x10, 0x02, ZBUS_MSG_POWER_EVT, "ZERO_DRAIN_SLEEP_ENTER", 22);
        case AI_ACTION_LAUNCH_WINDOWS_CONTAINER:
            return zbus_publish(0x10, 0x20, ZBUS_MSG_AI_INTENT, "CONTAINER_SPAWN_WINE", 20);
        case AI_ACTION_LAUNCH_ANDROID_CONTAINER:
            return zbus_publish(0x10, 0x20, ZBUS_MSG_AI_INTENT, "CONTAINER_SPAWN_WAYDROID", 24);
        default:
            return false;
    }
}
