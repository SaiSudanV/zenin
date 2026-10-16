/*
 * Project Zenin - Universal Real Binary Container Loader Subsystem
 * Implements true binary loaders for:
 *   1. Linux ELF64 (AArch64 native executables)
 *   2. Windows PE32+ (COFF/PE AArch64/ARM64 and x86_64 portable executables)
 *   3. macOS/iOS Mach-O 64 (Apple Darwin ARM64 load commands)
 *   4. Android APK / Native Activity Container (ZIP streaming + arm64-v8a .so)
 * Memory Budget: < 1.0 MB overhead per active container.
 */

#ifndef ZENIN_CONTAINER_LOADER_H
#define ZENIN_CONTAINER_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Container State & Type */
typedef enum {
    CONTAINER_LINUX_ELF64 = 1,
    CONTAINER_WINDOWS_PE32 = 2,
    CONTAINER_MACOS_MACHO64 = 3,
    CONTAINER_ANDROID_APK = 4
} zenin_container_type_t;

/* Loaded Binary Metadata Descriptor */
typedef struct {
    zenin_container_type_t type;
    const char *name;
    uintptr_t entry_point;      /* Address of real machine code execution */
    uintptr_t base_address;     /* Virtual/Physical base address in RAM */
    size_t loaded_image_size;   /* Actual bytes allocated in RAM */
    uint32_t total_sections;    /* Number of mapped segments */
    bool is_valid;
} zenin_loaded_binary_t;

/* Initialize Container Loader Subsystem */
void container_subsystem_init(void);

/* ========================================================================= */
/* 1. Linux ELF64 Binary Loader                                             */
/* ========================================================================= */
bool container_load_elf64(const void *file_buffer, size_t file_size, zenin_loaded_binary_t *out_bin);

/* ========================================================================= */
/* 2. Windows PE32+ Binary Loader                                            */
/* ========================================================================= */
bool container_load_pe32(const void *file_buffer, size_t file_size, zenin_loaded_binary_t *out_bin);

/* ========================================================================= */
/* 3. macOS / iOS Mach-O 64-bit Loader                                       */
/* ========================================================================= */
bool container_load_macho64(const void *file_buffer, size_t file_size, zenin_loaded_binary_t *out_bin);

/* ========================================================================= */
/* 4. Android APK Package Container Loader                                   */
/* ========================================================================= */
bool container_load_apk(const void *file_buffer, size_t file_size, zenin_loaded_binary_t *out_bin);

/* Execute loaded binary container in its isolated address context */
int container_execute(const zenin_loaded_binary_t *bin);

/* Run all 4 real binary container tests cleanly */
bool container_run_all_tests(void);

#endif /* ZENIN_CONTAINER_LOADER_H */

