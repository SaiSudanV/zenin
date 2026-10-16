/*
 * Project Zenin - Shell command/intent prototype.
 */

#include "../include/shell.h"
#include "../include/uart.h"
#include "../include/neural_core.h"
#include "../include/ai_core.h"
#include "../include/compat.h"

static bool str_equals(const char *s1, const char *s2) {
    if (!s1 || !s2) return false;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (*s1 == *s2);
}

void shell_init(void) {
    uart_puts("[zenin-shell] Command and intent functions initialized; UART CLI is not integrated.\n");
}

void shell_launch_app(zenin_app_id_t app_id) {
    switch (app_id) {
        case APP_ID_STATUS:
            uart_puts("[zenin-shell] -> Launching 'System Monitor':\n");
            uart_puts("              CPU utilization and power: not measured\n");
            uart_puts("              RAM: see boot-time kernel/PMM accounting\n");
            break;
        case APP_ID_AI_ASSISTANT:
            uart_puts("[zenin-shell] -> Launching 'Zenin AI Neural Engine':\n");
            uart_puts("              Experimental fixed-weight classifier; quality/latency unverified\n");
            break;
        case APP_ID_COMPAT_LAUNCHER:
            uart_puts("[zenin-shell] -> Launching 'Universal Multi-OS Compatibility Bridge':\n");
            uart_puts("              Format parsers exist; application execution is not implemented.\n");
            break;
        case APP_ID_TASK_MANAGER:
            uart_puts("[zenin-shell] Task-management service is not implemented.\n");
            break;
        case APP_ID_TERMINAL:
            uart_puts("[zenin-shell] -> Launching 'Zenin Direct Hardware Terminal'.\n");
            break;
        default:
            uart_puts("[zenin-shell] Unknown App ID.\n");
            break;
    }
}

void shell_execute_command(const char *cmd_line) {
    if (!cmd_line) return;

    uart_puts("zenin-shell$ ");
    uart_puts(cmd_line);
    uart_puts("\n");

    if (str_equals(cmd_line, "help")) {
        uart_puts("Commands: help, status, apps, ai <query>, run <app_type>\n");
        return;
    }

    if (str_equals(cmd_line, "status")) {
        shell_launch_app(APP_ID_STATUS);
        return;
    }

    if (str_equals(cmd_line, "apps")) {
        uart_puts("Installed System Apps:\n");
        uart_puts("  1. System Monitor\n");
        uart_puts("  2. Zenin AI Assistant\n");
        uart_puts("  3. Multi-OS Bridge (ELF/APK/EXE/Mach-O)\n");
        uart_puts("  4. Task Manager\n");
        uart_puts("  5. Direct Terminal\n");
        return;
    }

    /* Process natural language intent query with INT4 Neural Core */
    int8_t features[NEURAL_INPUT_DIM];
    neural_core_embed(cmd_line, features);
    int32_t confidence = 0;
    zenin_neural_intent_t intent = neural_core_predict(features, &confidence);

    uart_puts("[zenin-ai] Neural Core classified command into intent code: ");
    uart_putc('0' + (int)intent);
    uart_puts("\n");

    switch (intent) {
        case NEURAL_INTENT_PERF_OPTIMIZE:
            uart_puts("[zenin-ai] Intent classified as performance optimization; no DVFS actuator is implemented.\n");
            break;
        case NEURAL_INTENT_POWER_SAVE:
            uart_puts("[zenin-ai] Intent classified as power saving; no DVFS actuator is implemented.\n");
            break;
        case NEURAL_INTENT_LAUNCH_APP:
            uart_puts("[zenin-ai] Launch intent recognized; application runtime is not implemented.\n");
            shell_launch_app(APP_ID_COMPAT_LAUNCHER);
            break;
        case NEURAL_INTENT_SYS_STATUS:
            shell_launch_app(APP_ID_STATUS);
            break;
        default:
            uart_puts("[zenin-shell] Command evaluated.\n");
            break;
    }
}
