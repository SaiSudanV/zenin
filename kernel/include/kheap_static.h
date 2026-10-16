/*
 * Project Zenin - D011 hot-tier static slot arenas.
 *
 * Compile-time slot arrays with inline free-list pop/push: allocation
 * compiles to direct loads/stores on static storage (zero runtime
 * allocation instructions, no dynamic heap dependency).
 */

#ifndef ZENIN_KHEAP_STATIC_H
#define ZENIN_KHEAP_STATIC_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *slots;
    uint16_t slot_bytes;
    uint16_t slot_count;
    uint16_t free_top;
} kheap_static_t;

#define KHEAP_STATIC_SLOTS(name, slot_bytes_, slot_count_)                    \
    static uint8_t name##_slots[(slot_bytes_) * (slot_count_)]                \
        __attribute__((aligned(16)));                                         \
    static kheap_static_t name = { name##_slots, (slot_bytes_),               \
                                   (slot_count_), (slot_count_) }

static inline void *kheap_static_alloc(kheap_static_t *arena) {
    if (arena->free_top == 0) return NULL;
    arena->free_top--;
    return arena->slots + (size_t)arena->free_top * arena->slot_bytes;
}

static inline void kheap_static_free(kheap_static_t *arena) {
    if (arena->free_top >= arena->slot_count) return;
    arena->free_top++;
}

#endif /* ZENIN_KHEAP_STATIC_H */
