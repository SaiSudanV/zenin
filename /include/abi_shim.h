/*
 * Project Zenin - Universal Syscall Translator & Multi-OS ABI Shims
 * Directly translates POSIX (Linux), Bionic/Binder (Android), Win32 NT (Windows),
 * and Mach Traps / XNU (macOS) into native Zenin zero-overhead system calls.
 */

#ifndef ZENIN_ABI_SHIM_H
#define ZENIN_ABI_SHIM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Supported OS ABI Personalities */
typedef enum {
    ABI_PERSONALITY_ZENIN_NATIVE = 0,
    ABI_PERSONALITY_LINUX_POSIX,
    ABI_PERSONALITY_ANDROID_BIONIC,
    ABI_PERSONALITY_WIN32_NT,
    ABI_PERSONALITY_MACOS_MACH_XNU
} zenin_abi_personality_t;

/* Common translated syscall representation */
typedef struct {
    uint32_t syscall_num;
    uintptr_t arg1;
    uintptr_t arg2;
    uintptr_t arg3;
    uintptr_t arg4;
} zenin_raw_syscall_t;

/* Translated execution result */
typedef struct {
    int64_t return_value;
    bool success;
    const char *action_log;
} zenin_syscall_result_t;

/* Initialize universal ABI subsystem */
void abi_shim_init(void);

/* Translate and execute a POSIX/Linux Syscall */
zenin_syscall_result_t abi_dispatch_linux(uint32_t sysno, uintptr_t a1, uintptr_t a2, uintptr_t a3);

/* Translate and execute an Android Bionic / Binder transaction */
zenin_syscall_result_t abi_dispatch_android(uint32_t cmd, uintptr_t binder_ref, uintptr_t data_ptr);

/* Translate and execute a Win32 NT Syscall (ntdll / user32 / gdi32) */
zenin_syscall_result_t abi_dispatch_win32(uint32_t nt_call_id, uintptr_t handle, uintptr_t params);

/* Translate and execute a macOS Mach Trap / BSD Syscall (XNU kernel shim) */
zenin_syscall_result_t abi_dispatch_macos(int32_t mach_trap_id, uintptr_t port, uintptr_t msg_ptr);

#endif /* ZENIN_ABI_SHIM_H */
