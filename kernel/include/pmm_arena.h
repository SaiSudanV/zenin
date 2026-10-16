/*
 * Project Zenin - D011 hot-tier page arenas.
 *
 * Pre-bound, statically partitioned page storage for critical hot paths.
 * Allocation compiles to direct loads/stores on the arena descriptor and
 * backing array (zero runtime allocation instructions, zero dependency on
 * the dynamic PMM).
 */

#ifndef ZENIN_PMM_ARENA_H
#define ZENIN_PMM_ARENA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t *base;
    uint32_t page_count;
    uint32_t top;
} pmm_arena_t;

#define PMM_ARENA_DEFINE(name, pages)                                        \
    static uint8_t name##_backing[(pages) * 4096u] __attribute__((aligned(4096))); \
    static pmm_arena_t name = { (uint8_t *)name##_backing, (pages), (pages) }

static inline void *pmm_arena_alloc(pmm_arena_t *arena) {
    if (arena->top == 0) return NULL;
    arena->top--;
    return arena->base + (size_t)arena->top * 4096u;
}

static inline void pmm_arena_free(pmm_arena_t *arena) {
    if (arena->top >= arena->page_count) return;
    arena->top++;
}

static inline bool pmm_arena_owns(const pmm_arena_t *arena, const void *page) {
    const uint8_t *p = (const uint8_t *)page;
    return p >= arena->base && p < arena->base + (size_t)arena->page_count * 4096u;
}

#endif /* ZENIN_PMM_ARENA_H */
