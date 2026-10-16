/*
 * Relocatable, handle-addressed heap.
 *
 * Clients retain generation-checked handles rather than pointers, so live
 * objects can be compacted without invalidating references. Access pointers
 * are scoped to callbacks. This allocator is single-core until the kernel has
 * an SMP-safe lock. Data and descriptor pages are reclaimed when empty.
 */
#include "../include/kheap_handle.h"
#include "../include/pmm.h"

#define HANDLE_ARENA_BYTES (2u * 1024u * 1024u)
#define HANDLE_ALIGNMENT 16u
#define HANDLE_INDEX_BITS 17u
#define HANDLE_INDEX_MASK ((1u << HANDLE_INDEX_BITS) - 1u)
#define HANDLE_GENERATION_MAX ((1u << (32u - HANDLE_INDEX_BITS)) - 1u)
#define HANDLE_GENERATION_RETIRED 0x8000u
#define HANDLE_SLOT_NONE UINT32_MAX

typedef uint64_t alias_word_t __attribute__((__may_alias__));

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint32_t previous;
    uint32_t next;
} handle_slot_t;

#define HANDLE_SLOT_COUNT (HANDLE_ARENA_BYTES / HANDLE_ALIGNMENT - 1u)
#define HANDLE_SLOTS_PER_PAGE (PAGE_SIZE / sizeof(handle_slot_t))
#define HANDLE_DESCRIPTOR_PAGE_COUNT ((HANDLE_SLOT_COUNT + HANDLE_SLOTS_PER_PAGE - 1u) / HANDLE_SLOTS_PER_PAGE)

_Static_assert(sizeof(handle_slot_t) == 16u, "handle descriptor size changed");
_Static_assert(HANDLE_SLOT_COUNT <= HANDLE_INDEX_MASK,
               "handle index encoding is too small");

static uint8_t *arena;
static handle_slot_t *descriptor_pages[HANDLE_DESCRIPTOR_PAGE_COUNT];
/* Persistent generations prevent stale handles surviving arena reclamation. */
static uint16_t generations[HANDLE_SLOT_COUNT];
static uint32_t live_bytes;
static uint32_t packed_bytes;
static uint32_t active_head = HANDLE_SLOT_NONE;
static uint32_t active_tail = HANDLE_SLOT_NONE;
static uint32_t free_head = HANDLE_SLOT_NONE;
static uint32_t next_unused_slot;
static uint32_t allocated_descriptor_pages;
static bool in_access_callback;

static uint32_t align_size(size_t size) {
    return (uint32_t)((size + HANDLE_ALIGNMENT - 1u) & ~(size_t)(HANDLE_ALIGNMENT - 1u));
}

static handle_slot_t *slot_at(uint32_t index) {
    if (index >= HANDLE_SLOT_COUNT) return NULL;
    uint32_t page_index = index / HANDLE_SLOTS_PER_PAGE;
    if (!descriptor_pages[page_index]) return NULL;
    return &descriptor_pages[page_index][index % HANDLE_SLOTS_PER_PAGE];
}

static bool ensure_descriptor_page(uint32_t page_index) {
    if (page_index >= HANDLE_DESCRIPTOR_PAGE_COUNT) return false;
    if (descriptor_pages[page_index]) return true;
    if (page_index != allocated_descriptor_pages) return false;
    handle_slot_t *page = (handle_slot_t *)pmm_alloc_page();
    if (!page) return false;
    descriptor_pages[page_index] = page;
    allocated_descriptor_pages++;
    return true;
}

static kheap_handle_t make_handle(uint32_t index, uint16_t generation) {
    return ((uint32_t)generation << HANDLE_INDEX_BITS) | index;
}

static bool resolve(kheap_handle_t handle, uint32_t *index_out) {
    uint32_t index = handle & HANDLE_INDEX_MASK;
    uint32_t generation = handle >> HANDLE_INDEX_BITS;
    handle_slot_t *slot = slot_at(index);
    if (handle == KHEAP_HANDLE_NULL || index >= HANDLE_SLOT_COUNT ||
        generation == 0 || generation > HANDLE_GENERATION_MAX || !slot ||
        slot->size == 0 || generations[index] != generation) return false;
    if (index_out) *index_out = index;
    return true;
}

static void release_arenas_if_empty(void) {
    if (live_bytes != 0) return;
    uint8_t *released_arena = arena;
    arena = NULL;
    packed_bytes = 0;
    active_head = HANDLE_SLOT_NONE;
    active_tail = HANDLE_SLOT_NONE;
    free_head = HANDLE_SLOT_NONE;
    next_unused_slot = 0;
    for (uint32_t i = 0; i < allocated_descriptor_pages; i++) {
        handle_slot_t *page = descriptor_pages[i];
        descriptor_pages[i] = NULL;
        pmm_free_page(page);
    }
    allocated_descriptor_pages = 0;
    if (released_arena) pmm_free_pages(released_arena, HANDLE_ARENA_BYTES / PAGE_SIZE);
}

static bool acquire_data_arena(void) {
    if (arena) return true;
    arena = (uint8_t *)pmm_alloc_pages(HANDLE_ARENA_BYTES / PAGE_SIZE);
    return arena != NULL;
}

static void move_bytes(uint8_t *destination, const uint8_t *source, size_t length) {
    if (destination == source || length == 0) return;
    /* Compaction moves only toward lower addresses. Copy 16 bytes per step. */
    alias_word_t *dst_words = (alias_word_t *)destination;
    const alias_word_t *src_words = (const alias_word_t *)source;
    size_t words = length / sizeof(alias_word_t);
    for (size_t i = 0; i < words; i += 2) {
        alias_word_t low = src_words[i];
        alias_word_t high = i + 1u < words ? src_words[i + 1u] : 0;
        dst_words[i] = low;
        if (i + 1u < words) dst_words[i + 1u] = high;
    }
    size_t copied = words * sizeof(alias_word_t);
    for (size_t i = copied; i < length; i++) destination[i] = source[i];
}

/* Pack active objects in their original order, preserving each handle. */
static bool compact_arena(void) {
    uint32_t destination = 0;
    uint32_t current = active_head;
    while (current != HANDLE_SLOT_NONE) {
        handle_slot_t *slot = slot_at(current);
        if (!slot) return false;
        uint32_t old_offset = slot->offset;
        uint32_t storage = align_size(slot->size);
        uint32_t next = slot->next;
        if (destination > old_offset) return false;
        if (destination != old_offset) {
            move_bytes(arena + destination, arena + old_offset, slot->size);
        }
        slot->offset = destination;
        destination += storage;
        current = next;
    }
    packed_bytes = destination;
    return true;
}

static uint32_t take_free_slot(void) {
    if (free_head != HANDLE_SLOT_NONE) {
        uint32_t index = free_head;
        handle_slot_t *slot = slot_at(index);
        if (!slot) return HANDLE_SLOT_NONE;
        free_head = slot->next;
        slot->previous = HANDLE_SLOT_NONE;
        slot->next = HANDLE_SLOT_NONE;
        return index;
    }

    while (next_unused_slot < HANDLE_SLOT_COUNT) {
        uint32_t index = next_unused_slot;
        uint32_t page_index = index / HANDLE_SLOTS_PER_PAGE;
        if (!ensure_descriptor_page(page_index)) return HANDLE_SLOT_NONE;
        next_unused_slot++;
        uint16_t generation = generations[index];
        if (generation == HANDLE_GENERATION_RETIRED) continue;
        if (generation == 0) generations[index] = 1;
        handle_slot_t *slot = slot_at(index);
        slot->offset = 0;
        slot->size = 0;
        slot->previous = HANDLE_SLOT_NONE;
        slot->next = HANDLE_SLOT_NONE;
        return index;
    }
    return HANDLE_SLOT_NONE;
}

bool kheap_handle_alloc(size_t size, kheap_handle_t *out_handle) {
    if (!out_handle || size == 0 || in_access_callback || size > HANDLE_ARENA_BYTES) return false;
    *out_handle = KHEAP_HANDLE_NULL;
    uint32_t storage = align_size(size);
    if (storage < size || storage > HANDLE_ARENA_BYTES ||
        storage > HANDLE_ARENA_BYTES - live_bytes) return false;
    if (!acquire_data_arena()) return false;

    uint32_t index = take_free_slot();
    if (index == HANDLE_SLOT_NONE) {
        release_arenas_if_empty();
        return false;
    }
    if (packed_bytes != live_bytes && !compact_arena()) {
        handle_slot_t *failed_slot = slot_at(index);
        failed_slot->next = free_head;
        free_head = index;
        return false;
    }
    if (packed_bytes > HANDLE_ARENA_BYTES || storage > HANDLE_ARENA_BYTES - packed_bytes) {
        handle_slot_t *failed_slot = slot_at(index);
        failed_slot->next = free_head;
        free_head = index;
        return false;
    }

    handle_slot_t *slot = slot_at(index);
    slot->offset = packed_bytes;
    slot->size = (uint32_t)size;
    slot->previous = active_tail;
    slot->next = HANDLE_SLOT_NONE;
    if (active_tail == HANDLE_SLOT_NONE) active_head = index;
    else slot_at(active_tail)->next = index;
    active_tail = index;
    packed_bytes += storage;
    live_bytes += storage;
    *out_handle = make_handle(index, generations[index]);
    return true;
}

bool kheap_handle_free(kheap_handle_t handle) {
    uint32_t index;
    if (in_access_callback || !resolve(handle, &index)) return false;
    handle_slot_t *slot = slot_at(index);
    live_bytes -= align_size(slot->size);
    if (slot->previous == HANDLE_SLOT_NONE) active_head = slot->next;
    else slot_at(slot->previous)->next = slot->next;
    if (slot->next == HANDLE_SLOT_NONE) active_tail = slot->previous;
    else slot_at(slot->next)->previous = slot->previous;

    slot->offset = 0;
    slot->size = 0;
    slot->previous = HANDLE_SLOT_NONE;
    uint16_t generation = generations[index];
    if (generation == HANDLE_GENERATION_MAX) {
        generations[index] = HANDLE_GENERATION_RETIRED;
        slot->next = HANDLE_SLOT_NONE;
    } else {
        generations[index] = (uint16_t)(generation + 1u);
        slot->next = free_head;
        free_head = index;
    }
    release_arenas_if_empty();
    return true;
}

size_t kheap_handle_size(kheap_handle_t handle) {
    uint32_t index;
    return resolve(handle, &index) ? slot_at(index)->size : 0;
}

bool kheap_handle_access(kheap_handle_t handle,
                         kheap_handle_access_fn callback,
                         void *context) {
    uint32_t index;
    if (in_access_callback || !callback || !resolve(handle, &index)) return false;
    handle_slot_t *slot = slot_at(index);
    in_access_callback = true;
    callback(arena + slot->offset, slot->size, context);
    in_access_callback = false;
    return true;
}

size_t kheap_handle_capacity(void) { return HANDLE_ARENA_BYTES; }
size_t kheap_handle_max_objects(void) { return HANDLE_SLOT_COUNT; }
size_t kheap_handle_live_bytes(void) { return live_bytes; }
