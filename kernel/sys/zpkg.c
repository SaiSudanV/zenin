/*
 * Project Zenin - Package signature probe.
 * Archive extraction, filesystem mounting, and execution are not implemented.
 */

#include "../include/zpkg.h"
#include "../include/uart.h"

void zpkg_init(void) {
    uart_puts("[zenin-zpkg] Signature probe initialized; extraction/mounting unavailable.\n");
}

zenin_pkg_type_t zpkg_probe(const void *package_header_data, size_t header_len) {
    if (!package_header_data || header_len < 4) return PKG_TYPE_UNKNOWN;
    const uint8_t *bytes = (const uint8_t *)package_header_data;

    if (bytes[0] == 0x50 && bytes[1] == 0x4B &&
        (bytes[2] == 0x03 || bytes[2] == 0x05 || bytes[2] == 0x07) &&
        (bytes[3] == 0x04 || bytes[3] == 0x06 || bytes[3] == 0x08))
        return PKG_TYPE_ANDROID_APK;
    if (bytes[0] == 0x4D && bytes[1] == 0x5A) return PKG_TYPE_WINDOWS_EXE;
    if (bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F')
        return PKG_TYPE_LINUX_DEB_ELF;
    if ((bytes[0] == 0xCF && bytes[1] == 0xFA && bytes[2] == 0xED && bytes[3] == 0xFE) ||
        (bytes[0] == 0xFE && bytes[1] == 0xED && bytes[2] == 0xFA && bytes[3] == 0xCF))
        return PKG_TYPE_MACOS_APP_BUNDLE;
    return PKG_TYPE_UNKNOWN;
}

bool zpkg_unpack_and_mount(const char *package_path, const void *raw_data, size_t size,
                           zenin_package_manifest_t *out_manifest) {
    (void)package_path;
    (void)raw_data;
    (void)size;
    if (out_manifest) {
        out_manifest->type = PKG_TYPE_UNKNOWN;
        out_manifest->package_name = NULL;
        out_manifest->main_executable_path = NULL;
        out_manifest->uncompressed_asset_bytes = 0;
        out_manifest->icon_vram_color = 0;
        out_manifest->is_verified = false;
    }
    uart_puts("[zenin-zpkg] Refused: package extraction and filesystem mounting are unavailable.\n");
    return false;
}

bool zpkg_execute_manifest(const zenin_package_manifest_t *manifest) {
    (void)manifest;
    uart_puts("[zenin-zpkg] Refused: no process loader or application runtime is available.\n");
    return false;
}
