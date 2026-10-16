/*
 * Project Zenin - Physical Memory Manager (PMM) Implementation
 * Ultra-fast bitwise page allocator.
 */

#include "../include/pmm.h"
#include "../include/uart.h"

static uint8_t *bitmap = NULL;
static size_t total_pages = 0;
static size_t free_pages = 0;
static uintptr_t ram_base = 0;

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
    ram_base = (mem_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    total_pages = mem_size / PAGE_SIZE;
    free_pages = total_pages;

    /* Place bitmap at the very start of managed memory */
    bitmap = (uint8_t *)ram_base;
    size_t bitmap_size = (total_pages + 7) / 8;
    size_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Initially mark all pages as free (0) */
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0;
    }

    /* Reserve the pages that hold the bitmap itself */
    for (size_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(i);
        free_pages--;
    }
}

void *pmm_alloc_page(void) {
    if (free_pages == 0) {
        return NULL; /* Out of physical memory */
    }

    /* Fast bitwise scan */
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages--;
            return (void *)(ram_base + (i * PAGE_SIZE));
        }
    }

    return NULL;
}

void *pmm_alloc_pages(size_t count) {
    if (count == 0 || free_pages < count) return NULL;
    if (count == 1) return pmm_alloc_page();

    size_t consecutive = 0;
    size_t start_idx = 0;

    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_idx = i;
            consecutive++;
            if (consecutive == count) {
                for (size_t j = start_idx; j < start_idx + count; j++) {
                    bitmap_set(j);
                }
                free_pages -= count;
                return (void *)(ram_base + (start_idx * PAGE_SIZE));
            }
        } else {
            consecutive = 0;
        }
    }

    return NULL;
}

void pmm_free_page(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < ram_base) return;

    size_t page_idx = (addr - ram_base) / PAGE_SIZE;
    if (page_idx < total_pages && bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        free_pages++;
    }
}

void pmm_free_pages(void *ptr, size_t count) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < ram_base || count == 0) return;

    size_t start_idx = (addr - ram_base) / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        size_t idx = start_idx + i;
        if (idx < total_pages && bitmap_test(idx)) {
            bitmap_clear(idx);
            free_pages++;
        }
    }
}

size_t pmm_get_free_pages(void) {
    return free_pages;
}

size_t pmm_get_total_pages(void) {
    return total_pages;
}
