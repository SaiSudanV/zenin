/*
 * Project Zenin - Universal Multi-OS Subsystem & Binary Loader
 * Dispatches and launches native Linux (ELF), Android (.apk), Windows (.exe), and macOS (Mach-O) binaries.
 * Enforces Zero-Idle Resource Reclamation (Memory drops back to 0 MB on app exit).
 */

#ifndef ZENIN_COMPAT_H
#define ZENIN_COMPAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Universal Binary Format Signatures */
typedef enum {
    BIN_FORMAT_UNKNOWN = 0,
    BIN_FORMAT_LINUX_ELF,      /* Native Linux ELF (0x7F 'E' 'L' 'F') */
    BIN_FORMAT_ANDROID_APK,     /* Android Zip/APK (0x50 0x4B 0x03 0x04) */
    BIN_FORMAT_WINDOWS_PE,      /* Windows Portable Executable ('M' 'Z') */
    BIN_FORMAT_MACOS_MACHO      /* macOS Mach-O 64-bit (0xFE 0xED 0xFA 0xCF) */
} zenin_bin_format_t;

typedef struct {
    zenin_bin_format_t format;
    const char *format_name;
    const char *runtime_subsystem;
    size_t memory_budget_mb;
    bool is_active;
} zenin_app_process_t;

/* Initialize universal compatibility launcher */
void compat_launcher_init(void);

/* Identify binary format from file magic bytes or file extension */
zenin_bin_format_t compat_identify_binary(const char *filename_or_magic);

/* Launch application into its isolated on-demand environment */
bool compat_launch_app(const char *app_path, zenin_app_process_t *out_proc);

/* Terminate application and reclaim 100% of memory back to kernel pool */
bool compat_terminate_app(zenin_app_process_t *proc);

#endif /* ZENIN_COMPAT_H */
