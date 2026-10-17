#ifndef ZENIN_MEMORY_ACCOUNTING_H
#define ZENIN_MEMORY_ACCOUNTING_H

#include <stdint.h>

#define ZENIN_ACCOUNTING_PAGE_SIZE 4096ULL

static inline int zenin_checked_add_u64(uint64_t a, uint64_t b,
                                        uint64_t *out) {
    if (!out || a > UINT64_MAX - b) return 0;
    *out = a + b;
    return 1;
}

static inline int zenin_pages_to_bytes(uint64_t pages, uint64_t *out) {
    if (!out || pages > UINT64_MAX / ZENIN_ACCOUNTING_PAGE_SIZE) return 0;
    *out = pages * ZENIN_ACCOUNTING_PAGE_SIZE;
    return 1;
}

/* OS-owned resident bytes are the loaded kernel image plus live PMM pages.
 * PMM is initialized strictly after __kernel_end, so the ranges are disjoint. */
static inline int zenin_os_owned_resident_bytes(uint64_t image_start,
                                                uint64_t image_end,
                                                uint64_t used_pages,
                                                uint64_t *out) {
    uint64_t pmm_bytes;
    if (!out || image_end <= image_start ||
        !zenin_pages_to_bytes(used_pages, &pmm_bytes)) return 0;
    return zenin_checked_add_u64(image_end - image_start, pmm_bytes, out);
}

/* The existing report defines platform-reserved as the span below the
 * kernel load address. A firmware usable-RAM base above that address does
 * not define such a span; return invalid instead of unsigned wraparound. */
static inline int zenin_platform_reserved_before_kernel(uint64_t kernel_start,
                                                         uint64_t ram_base,
                                                         uint64_t *out) {
    if (!out || ram_base > kernel_start) return 0;
    *out = kernel_start - ram_base;
    return 1;
}

static inline int zenin_idle_ram_within_budget(uint64_t resident_bytes,
                                                uint64_t budget_bytes) {
    return resident_bytes < budget_bytes;
}

#endif
