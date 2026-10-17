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
#define ZENIN_BOOT_INFO_MAX_REGIONS 512

/* A contiguous physical range in 4 KiB pages. Used for both usable RAM and
 * memory-mapped I/O (device) ranges. */
typedef struct {
    uint64_t base;   /* physical start (page-aligned) */
    uint64_t pages;  /* length in 4 KiB pages */
} zenin_boot_range_t;

#define ZENIN_BOOT_PAGE_SIZE 4096ULL
#define ZENIN_BOOT_ADDRESS_LIMIT (1ULL << 48)

/* Convert a firmware page range to a checked half-open byte range. */
static inline int zenin_boot_range_end(const zenin_boot_range_t *range,
                                       uint64_t *end_out) {
    if (!range || !end_out || range->pages == 0 ||
        (range->base & (ZENIN_BOOT_PAGE_SIZE - 1)) != 0 ||
        range->pages > UINT64_MAX / ZENIN_BOOT_PAGE_SIZE) {
        return 0;
    }
    uint64_t bytes = range->pages * ZENIN_BOOT_PAGE_SIZE;
    if (range->base > UINT64_MAX - bytes) return 0;
    *end_out = range->base + bytes;
    return *end_out > range->base;
}

static inline int zenin_boot_ranges_overlap(const zenin_boot_range_t *a,
                                             const zenin_boot_range_t *b) {
    uint64_t a_end, b_end;
    if (!zenin_boot_range_end(a, &a_end) ||
        !zenin_boot_range_end(b, &b_end)) return 1;
    return a->base < b_end && b->base < a_end;
}

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

/* Validate bounded, nonempty, page-aligned, nonoverlapping firmware ranges. */
enum {
    ZENIN_BOOT_INFO_VALID = 0,
    ZENIN_BOOT_INFO_BAD_COUNTS = 1,
    ZENIN_BOOT_INFO_BAD_KERNEL_RANGE = 2,
    ZENIN_BOOT_INFO_BAD_RAM_RANGE = 3,
    ZENIN_BOOT_INFO_OVERLAPPING_RAM = 4,
    ZENIN_BOOT_INFO_BAD_MMIO_RANGE = 5,
    ZENIN_BOOT_INFO_OVERLAPPING_MMIO = 6,
    ZENIN_BOOT_INFO_RAM_MMIO_OVERLAP = 7
};

static inline int zenin_boot_info_error(const zenin_boot_info_t *b) {
    if (!b || b->ram_count == 0 ||
        b->ram_count > ZENIN_BOOT_INFO_MAX_REGIONS ||
        b->mmio_count > ZENIN_BOOT_INFO_MAX_REGIONS) {
        return ZENIN_BOOT_INFO_BAD_COUNTS;
    }
    if (b->kernel_start >= b->kernel_end) {
        return ZENIN_BOOT_INFO_BAD_KERNEL_RANGE;
    }

    const zenin_boot_range_t *ram = zenin_boot_ram(b);
    const zenin_boot_range_t *mmio = zenin_boot_mmio(b);
    for (uint64_t i = 0; i < b->ram_count; i++) {
        uint64_t end;
        if (!zenin_boot_range_end(&ram[i], &end) ||
            end > ZENIN_BOOT_ADDRESS_LIMIT) return ZENIN_BOOT_INFO_BAD_RAM_RANGE;
        for (uint64_t j = 0; j < i; j++) {
            if (zenin_boot_ranges_overlap(&ram[i], &ram[j])) {
                return ZENIN_BOOT_INFO_OVERLAPPING_RAM;
            }
        }
        for (uint64_t j = 0; j < b->mmio_count; j++) {
            uint64_t mmio_end;
            if (!zenin_boot_range_end(&mmio[j], &mmio_end) ||
                mmio_end > ZENIN_BOOT_ADDRESS_LIMIT) {
                return ZENIN_BOOT_INFO_BAD_MMIO_RANGE;
            }
            if (zenin_boot_ranges_overlap(&ram[i], &mmio[j])) {
                return ZENIN_BOOT_INFO_RAM_MMIO_OVERLAP;
            }
        }
    }
    for (uint64_t i = 0; i < b->mmio_count; i++) {
        uint64_t end;
        if (!zenin_boot_range_end(&mmio[i], &end) ||
            end > ZENIN_BOOT_ADDRESS_LIMIT) return ZENIN_BOOT_INFO_BAD_MMIO_RANGE;
        for (uint64_t j = 0; j < i; j++) {
            if (zenin_boot_ranges_overlap(&mmio[i], &mmio[j])) {
                return ZENIN_BOOT_INFO_OVERLAPPING_MMIO;
            }
        }
    }
    return ZENIN_BOOT_INFO_VALID;
}

static inline int zenin_boot_info_ranges_valid(const zenin_boot_info_t *b) {
    return zenin_boot_info_error(b) == ZENIN_BOOT_INFO_VALID;
}

/* True only when a complete, structurally valid descriptor is present. */
static inline int zenin_boot_info_valid(const void *p) {
    if (!p) return 0;
    const zenin_boot_info_t *b = (const zenin_boot_info_t *)p;
    return b->magic == ZENIN_BOOT_INFO_MAGIC &&
           zenin_boot_info_ranges_valid(b);
}

#endif /* ZENIN_BOOT_INFO_H */
