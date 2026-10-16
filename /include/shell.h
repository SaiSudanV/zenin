/*
 * Project Zenin - Interactive Native Shell & App Launcher
 * Zero-overhead CLI & Touch GUI command processor.
 */

#ifndef ZENIN_SHELL_H
#define ZENIN_SHELL_H

#include <stdint.h>
#include <stdbool.h>

/* Built-in system app IDs */
typedef enum {
    APP_ID_STATUS = 1,
    APP_ID_AI_ASSISTANT,
    APP_ID_COMPAT_LAUNCHER,
    APP_ID_TASK_MANAGER,
    APP_ID_TERMINAL
} zenin_app_id_t;

/* Initialize shell environment */
void shell_init(void);

/* Process user command string (keyboard/touch/intent) */
void shell_execute_command(const char *cmd_line);

/* Launch native system application */
void shell_launch_app(zenin_app_id_t app_id);

#endif /* ZENIN_SHELL_H */
