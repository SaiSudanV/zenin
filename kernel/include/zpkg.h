/* Project Zenin package signature probe. Extraction/mounting are unimplemented. */

#ifndef ZENIN_ZPKG_H
#define ZENIN_ZPKG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "compat.h"

/* Magic byte signatures for universal packages */
#define ZPKG_MAGIC_ZIP   0x04034B50  /* 'PK\x03\x04' - Android APK / iOS IPA */
#define ZPKG_MAGIC_PE    0x00005A4D  /* 'MZ' - Windows PE Portable Executable */
#define ZPKG_MAGIC_ELF   0x464C457F  /* '\x7fELF' - Linux Executable */
#define ZPKG_MAGIC_MACHO 0xFEEDFACF  /* macOS Mach-O 64-bit Big-Endian / Little-Endian */

typedef enum {
    PKG_TYPE_UNKNOWN = 0,
    PKG_TYPE_ANDROID_APK,
    PKG_TYPE_WINDOWS_EXE,
    PKG_TYPE_LINUX_DEB_ELF,
    PKG_TYPE_MACOS_APP_BUNDLE
} zenin_pkg_type_t;

typedef struct {
    zenin_pkg_type_t type;
    const char *package_name;
    const char *main_executable_path;
    size_t uncompressed_asset_bytes;
    uint32_t icon_vram_color;
    bool is_verified;
} zenin_package_manifest_t;

/* Initialize universal package loader */
void zpkg_init(void);

/* Probe package buffer magic bytes and identify file type */
zenin_pkg_type_t zpkg_probe(const void *package_header_data, size_t header_len);

/* Currently returns false: no filesystem/archive backend is implemented. */
bool zpkg_unpack_and_mount(const char *package_path, const void *raw_data, size_t size, zenin_package_manifest_t *out_manifest);

/* Currently returns false: no process loader/runtime is implemented. */
bool zpkg_execute_manifest(const zenin_package_manifest_t *manifest);

#endif /* ZENIN_ZPKG_H */
