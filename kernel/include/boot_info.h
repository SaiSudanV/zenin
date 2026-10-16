/*
 * Project Zenin — boot handoff descriptor (Stream C).
 *
 * The UEFI stub (scripts/efi_boot.c) discovers the real memory map via
 * GetMemoryMap and passes this descriptor to the kernel in x0 (preserved by
 * boot.S across early setup). arch_mmu_enable() and kmain() read it to build a
 * memory-map-driven identity map and to size the PMM against real RAM instead
 * of a hardcoded QEMU layout — required for the Goal 1.6 idle-RAM measurement
 * on actual target hardware.
 *
 * Layout is fixed and shared with the standalone UEFI stub; the magic lets the
 * kernel reject a stale/absent descriptor and fall back safely to QEMU virt.
 */

#ifndef ZENIN_BOOT_INFO_H
#define ZENIN_BOOT_INFO_H

#include <stdint.h>

#define ZENIN_BOOT_INFO_MAGIC 0x5A454E49424F4F54ULL /* "ZENIBOOT" */
#define ZENIN_BOOT_INFO_MAX_REGIONS 64

/* A contiguous physical range in 4 KiB pages. Used for both usable RAM and
 * memory-mapped I/O (device) ranges. */
typedef struct {
    uint64_t base;   /* physical start (page-aligned) */
    uint64_t pages;  /* length in 4 KiB pages */
} zenin_boot_range_t;

/*
 * Boot descriptor. The kernel image is loaded at [kernel_start, kernel_end).
 * Usable RAM ranges exclude the loaded image and firmware-reserved memory
 * (memory-map type != conventional/boot-services). MMIO ranges are the
 * device windows (GIC, UART, ...) the identity map must map as device memory.
 */
typedef struct {
    uint64_t magic;          /* ZENIN_BOOT_INFO_MAGIC */
    uint64_t kernel_start;
    uint64_t kernel_end;
    uint64_t ram_count;      /* usable RAM ranges */
    uint64_t mmio_count;     /* device (MMIO) ranges */
    /* zenin_boot_range_t ram[ram_count] then zenin_boot_range_t mmio[mmio_count]
     * are stored inline, immediately after this header. */
} zenin_boot_info_t;

static inline const zenin_boot_range_t *zenin_boot_ram(const zenin_boot_info_t *b) {
    return (const zenin_boot_range_t *)(b + 1);
}
static inline const zenin_boot_range_t *zenin_boot_mmio(const zenin_boot_info_t *b) {
    return (const zenin_boot_range_t *)(b + 1) + ZENIN_BOOT_INFO_MAX_REGIONS;
}

/* True when a valid descriptor is present at `p` (safe to read the fields). */
static inline int zenin_boot_info_valid(const void *p) {
    return p != 0 && ((const zenin_boot_info_t *)p)->magic == ZENIN_BOOT_INFO_MAGIC;
}

#endif /* ZENIN_BOOT_INFO_H */
