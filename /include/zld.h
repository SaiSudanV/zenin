/*
 * Project Zenin - Dynamic Symbol Linker (z-ld.so) & Universal C Runtime Shims
 * Provides zero-overhead symbol resolution for Android Bionic, Linux Glibc,
 * Win32 MSVCRT, and macOS Libsystem dynamically loaded binaries (.so, .dll, .dylib).
 * Peak Resolution Time: < 5 microseconds | Overhead: < 64 KB RAM.
 */

#ifndef ZENIN_ZLD_H
#define ZENIN_ZLD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Target ABI Runtime Family */
typedef enum {
    ZLD_ABI_GENERIC = 0,
    ZLD_ABI_BIONIC,     /* Android libc.so, libm.so, libdl.so */
    ZLD_ABI_GLIBC,      /* Linux libc.so.6, libpthread.so.0 */
    ZLD_ABI_MSVCRT,     /* Windows MSVCRT.dll, KERNEL32.dll */
    ZLD_ABI_LIBSYSTEM   /* macOS libSystem.B.dylib */
} zld_abi_t;

/* Dynamic symbol entry */
typedef struct {
    const char *name;
    uintptr_t func_ptr;
    zld_abi_t abi;
} zld_symbol_entry_t;

/* Initialize the dynamic symbol linker table */
void zld_init(void);

/* Register an internal kernel or runtime function as an exportable symbol */
bool zld_register_symbol(const char *name, uintptr_t address, zld_abi_t abi);

/* Resolve an unresolved dynamic symbol from a binary */
uintptr_t zld_resolve_symbol(const char *name, zld_abi_t requested_abi);

/* Relocate and resolve an entire symbol dependency table of a shared object */
uint32_t zld_link_dependencies(const char *module_name, const char **needed_symbols, uint32_t count, zld_abi_t abi);

/* Query dynamic linker metrics */
void zld_get_metrics(uint32_t *out_registered_syms, uint32_t *out_resolved_count, size_t *out_memory_used);

#endif /* ZENIN_ZLD_H */
