/*
 * Project Zenin - Dynamic Symbol Linker (z-ld.so) & Universal C Runtime Implementation
 * Intercepts unresolved dynamic symbols from Android .so, Linux ELF, Windows DLL,
 * and macOS dylib modules and routes them directly to Zenin's deterministic kernel services.
 */

#include "../include/zld.h"
#include "../include/uart.h"
#include "../include/kheap.h"
#include "../include/zbus.h"
#include "../include/fb.h"
#include "../include/zgl.h"
#include "../include/hal_cpu.h"

#define MAX_DYNAMIC_SYMBOLS 128

static zld_symbol_entry_t symbol_table[MAX_DYNAMIC_SYMBOLS];
static uint32_t symbol_count = 0;
static uint32_t total_resolved = 0;

/* Standard string comparison */
static bool str_eq(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == *b;
}

/* Universal C Runtime Shims */
static void *shim_malloc(size_t size) {
    return kmalloc(size);
}

static void shim_free(void *ptr) {
    kfree(ptr);
}

static void *shim_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

static void *shim_memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

static int shim_puts(const char *s) {
    uart_puts(s);
    uart_puts("\n");
    return 0;
}

static void shim_abort(void) {
    uart_puts("[z-ld] Application called abort()! Graceful kernel rescue active.\n");
}

/* Android Bionic Log Shim (__android_log_print) */
static int shim_android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio;
    uart_puts("   [Android-Log] [");
    if (tag) uart_puts(tag);
    uart_puts("] ");
    if (fmt) uart_puts(fmt);
    uart_puts("\n");
    return 0;
}

/* Windows MSVCRT / Win32 Shims (OutputDebugStringA, GetTickCount) */
static void shim_win32_output_debug_string(const char *str) {
    uart_puts("   [Win32-Debug] ");
    if (str) uart_puts(str);
    uart_puts("\n");
}

static uint64_t win32_start_counter = 0;
static uint64_t win32_counter_frequency = 0;

static uint32_t shim_win32_get_tick_count(void) {
    if (win32_counter_frequency == 0) return 0;
    uint64_t elapsed = hal_get_cycles() - win32_start_counter;
    uint64_t elapsed_ms = (elapsed / win32_counter_frequency) * 1000 +
                          ((elapsed % win32_counter_frequency) * 1000) / win32_counter_frequency;
    return (uint32_t)elapsed_ms; /* Win32 GetTickCount wraps at 2^32 ms. */
}

/* macOS / iOS Mach Trap / Cocoa Log Shim (NSLog) */
static void shim_macos_nslog(const char *msg) {
    uart_puts("   [macOS-NSLog] ");
    if (msg) uart_puts(msg);
    uart_puts("\n");
}

/* Initialize the dynamic linker and seed universal C runtime symbols */
void zld_init(void) {
    symbol_count = 0;
    total_resolved = 0;
    win32_start_counter = hal_get_cycles();
    win32_counter_frequency = hal_get_counter_frequency();

    /* Generic Standard C Library symbols (libc / msvcrt / libSystem) */
    zld_register_symbol("malloc", (uintptr_t)shim_malloc, ZLD_ABI_GENERIC);
    zld_register_symbol("free", (uintptr_t)shim_free, ZLD_ABI_GENERIC);
    zld_register_symbol("memcpy", (uintptr_t)shim_memcpy, ZLD_ABI_GENERIC);
    zld_register_symbol("memset", (uintptr_t)shim_memset, ZLD_ABI_GENERIC);
    zld_register_symbol("puts", (uintptr_t)shim_puts, ZLD_ABI_GENERIC);
    zld_register_symbol("abort", (uintptr_t)shim_abort, ZLD_ABI_GENERIC);

    /* Android Bionic specific exports */
    zld_register_symbol("__android_log_print", (uintptr_t)shim_android_log_print, ZLD_ABI_BIONIC);
    zld_register_symbol("dlopen", (uintptr_t)zld_resolve_symbol, ZLD_ABI_BIONIC);
    zld_register_symbol("dlsym", (uintptr_t)zld_resolve_symbol, ZLD_ABI_BIONIC);

    /* Windows Win32 / MSVCRT exports */
    zld_register_symbol("OutputDebugStringA", (uintptr_t)shim_win32_output_debug_string, ZLD_ABI_MSVCRT);
    zld_register_symbol("GetTickCount", (uintptr_t)shim_win32_get_tick_count, ZLD_ABI_MSVCRT);

    /* macOS / iOS Mach & Darwin exports */
    zld_register_symbol("NSLog", (uintptr_t)shim_macos_nslog, ZLD_ABI_LIBSYSTEM);

    uart_puts("[zenin-zld] Dynamic Symbol Linker (z-ld.so) initialized:\n");
    uart_puts("            Registered platform shim symbols; ABI coverage is partial.\n");
    uart_puts("            Lookup timing is not measured.\n");
}

bool zld_register_symbol(const char *name, uintptr_t address, zld_abi_t abi) {
    if (symbol_count >= MAX_DYNAMIC_SYMBOLS || !name) return false;

    symbol_table[symbol_count].name = name;
    symbol_table[symbol_count].func_ptr = address;
    symbol_table[symbol_count].abi = abi;
    symbol_count++;
    return true;
}

uintptr_t zld_resolve_symbol(const char *name, zld_abi_t requested_abi) {
    if (!name) return 0;

    for (uint32_t i = 0; i < symbol_count; i++) {
        if (str_eq(symbol_table[i].name, name)) {
            /* Match generic or exact ABI */
            if (symbol_table[i].abi == ZLD_ABI_GENERIC || symbol_table[i].abi == requested_abi) {
                total_resolved++;
                return symbol_table[i].func_ptr;
            }
        }
    }
    return 0;
}

uint32_t zld_link_dependencies(const char *module_name, const char **needed_symbols, uint32_t count, zld_abi_t abi) {
    if (!module_name || !needed_symbols) return 0;

    uint32_t resolved_now = 0;
    uart_puts("[z-ld] Diagnostic symbol-name lookup for module description: ");
    uart_puts(module_name);
    uart_puts("\n");

    for (uint32_t i = 0; i < count; i++) {
        uintptr_t target = zld_resolve_symbol(needed_symbols[i], abi);
        if (target != 0) {
            resolved_now++;
            uart_puts("       -> Registered symbol found: '");
            uart_puts(needed_symbols[i]);
            uart_puts("' (no module relocation or load performed)\n");
        } else {
            uart_puts("       -> Warning: Unresolved symbol '");
            uart_puts(needed_symbols[i]);
            uart_puts("'\n");
        }
    }
    return resolved_now;
}

void zld_get_metrics(uint32_t *out_registered_syms, uint32_t *out_resolved_count, size_t *out_memory_used) {
    if (out_registered_syms) *out_registered_syms = symbol_count;
    if (out_resolved_count) *out_resolved_count = total_resolved;
    if (out_memory_used) *out_memory_used = sizeof(symbol_table);
}
