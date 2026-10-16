/*
 * Project Zenin — boot handoff descriptor (Stream C).
 *
 * The UEFI stub (scripts/efi_boot.c) discovers the real memory map via
 * GetMemoryMap and passes this descriptor to the kernel in x0 (preserved by
 * boot.S across early setup). kmain reads it to size the PMM against the real
 * RAM instead of a hardcoded QEMU layout — required for the Goal 1.6 idle-RAM
 * measurement on actual target hardware.
 *
 * Layout is fixed and shared with the standalone UEFI stub; the magic lets the
 * kernel reject a stale/absent descriptor and fall back safely.
 */

#ifndef ZENIN_BOOT_INFO_H
#define ZENIN_BOOT_INFO_H

#include <stdint.h>

#define ZENIN_BOOT_INFO_MAGIC 0x5A454E49424F4F54ULL /* "ZENIBOOT" */

/* One usable RAM region discovered from the firmware memory map. */
typedef struct {
    uint64_t base;   /* physical start (page-aligned) */
    uint64_t pages;  /* length in 4 KiB pages */
} zenin_ram_region_t;

/*
 * Boot descriptor. The kernel is loaded at [kernel_start, kernel_end). Usable
 * RAM regions exclude the loaded kernel image and any firmware-reserved ranges
 * (memory-map type != conventional/boot-services).
 */
typedef struct {
    uint64_t magic;          /* ZENIN_BOOT_INFO_MAGIC */
    uint64_t kernel_start;   /* where the kernel image was loaded */
    uint64_t kernel_end;
    uint64_t ram_count;      /* number of usable RAM regions that follow */
    /* zenin_ram_region_t regions[ram_count] inline after this header */
} zenin_boot_info_t;

#define ZENIN_BOOT_INFO_MAX_REGIONS 64

#endif /* ZENIN_BOOT_INFO_H */
