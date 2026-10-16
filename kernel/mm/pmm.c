/*
 * Project Zenin - Physical Memory Manager (PMM) Implementation
 * Ultra-fast bitwise page allocator.
 *
 * D011 two-tier model: the single-page hot path is the minimal bitmap
 * scan/pop/set with no accounting counters, telemetry, or non-essential
 * checks. Free-page accounting is derived from the bitmap on query, so it
 * equals the actual state by construction (INVARIANT 2).
 */

#include "../include/pmm.h"
#include "../include/uart.h"

static uint8_t *bitmap = NULL;
static size_t total_pages = 0;
static uintptr_t ram_base = 0;
static size_t bitmap_pages_reserved = 0;
static size_t allocation_cursor = 0;

/* Helper bit operations */
static inline void bitmap_set(size_t page_idx) {
    bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static inline void bitmap_clear(size_t page_idx) {
    bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static inline bool bitmap_test(size_t page_idx) {
    return (bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

void pmm_init(uintptr_t mem_start, size_t mem_size) {
    /* mem_size describes [mem_start, mem_start + mem_size), not total RAM. */
    bitmap = NULL;
    total_pages = 0;
    bitmap_pages_reserved = 0;
    allocation_cursor = 0;
    if (mem_size == 0 || mem_start > UINTPTR_MAX - mem_size) return;

    uintptr_t mem_end = mem_start + mem_size;
    if (mem_start > UINTPTR_MAX - (PAGE_SIZE - 1)) return;
    ram_base = (mem_start + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    if (ram_base >= mem_end) return;
    total_pages = (mem_end - ram_base) / PAGE_SIZE;
    if (total_pages > SIZE_MAX - 7) {
        total_pages = 0;
        return;
    }

    /* Place bitmap at the very start of managed memory */
    bitmap = (uint8_t *)ram_base;
    size_t bitmap_size = (total_pages + 7) / 8;
    size_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (bitmap_pages > total_pages) {
        bitmap = NULL;
        total_pages = 0;
        return;
    }
    bitmap_pages_reserved = bitmap_pages;

    /* Initially mark all pages as free (0) */
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }

    /* Reserve the pages that hold the bitmap itself */
    for (size_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(i);
    }
}

/* Minimal bitmap pop (D011 cold-tier baseline): next-fit scan, set, cursor.
 * Caller contract: pmm_init has run. */
void *pmm_alloc_page(void) {
    for (size_t pass = 0; pass < 2; pass++) {
        size_t begin = pass == 0 ? allocation_cursor : 0;
        size_t end = pass == 0 ? total_pages : allocation_cursor;
        for (size_t i = begin; i < end; i++) {
            if (!bitmap_test(i)) {
                bitmap_set(i);
                allocation_cursor = (i + 1 == total_pages) ? 0 : i + 1;
                return (void *)(ram_base + (i * PAGE_SIZE));
            }
        }
    }

    return NULL;
}

void *pmm_alloc_pages(size_t count) {
    if (count == 0 || count > total_pages) return NULL;
    if (count == 1) return pmm_alloc_page();

    for (size_t pass = 0; pass < 2; pass++) {
        size_t begin = pass == 0 ? allocation_cursor : 0;
        size_t end = total_pages;
        size_t run = 0;
        size_t run_start = begin;
        for (size_t i = begin; i < end; i++) {
            if (!bitmap_test(i)) {
                if (run == 0) run_start = i;
                if (++run == count) {
                    for (size_t j = run_start; j <= i; j++) bitmap_set(j);
                    allocation_cursor = (i + 1 == total_pages) ? 0 : i + 1;
                    return (void *)(ram_base + (run_start * PAGE_SIZE));
                }
            } else {
                run = 0;
            }
        }
    }

    return NULL;
}

/* Minimal bitmap clear with essential validation (invalid and double frees
 * must be rejected — INVARIANT 13 and the PMM regression enforce this; the
 * check is essential correctness, not removable bookkeeping). */
void pmm_free_page(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < ram_base || (addr - ram_base) % PAGE_SIZE != 0) return;

    size_t page_idx = (addr - ram_base) / PAGE_SIZE;
    if (page_idx >= bitmap_pages_reserved && page_idx < total_pages &&
        bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        if (page_idx < allocation_cursor) allocation_cursor = page_idx;
    }
}

void pmm_free_pages(void *ptr, size_t count) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < ram_base || (addr - ram_base) % PAGE_SIZE != 0 || count == 0) return;

    size_t start_idx = (addr - ram_base) / PAGE_SIZE;
    if (start_idx < bitmap_pages_reserved || start_idx >= total_pages || count > total_pages - start_idx) return;
    for (size_t i = 0; i < count; i++) {
        if (!bitmap_test(start_idx + i)) return;
    }
    for (size_t i = 0; i < count; i++) {
        bitmap_clear(start_idx + i);
    }
    if (start_idx < allocation_cursor) allocation_cursor = start_idx;
}

/* Derived accounting (D011): equals the actual bitmap state by construction. */
size_t pmm_get_free_pages(void) {
    size_t used = 0;
    for (size_t i = 0; i < total_pages; i++) {
        if (bitmap_test(i)) used++;
    }
    return total_pages - used;
}

size_t pmm_get_total_pages(void) {
    return total_pages;
}
