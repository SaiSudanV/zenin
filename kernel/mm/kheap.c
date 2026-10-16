/*
 * Project Zenin page-mapped kernel allocator (D010 fragmentation-proof).
 *
 * Requests through 4064 bytes use size-class slabs. Larger requests receive a
 * contiguous virtual range backed by independently allocated PMM pages. No
 * allocation asks the PMM for a multi-page contiguous run. This removes
 * physical-page external fragmentation from the raw-pointer heap paths.
 *
 * D010 zero-fragmentation property: the 96 TiB heap window is bookkept as a
 * sorted list of allocated spans only (free space is the gaps between spans,
 * so every freed range is instantly reusable). Each allocated window page is
 * backed 1:1 by a physical page, so at most P pages are ever allocated. For
 * requests of n <= n_max pages, blocking every n-page gap would require
 * floor(W/n) >= floor(W/n_max) = 786432 allocated pages, which exceeds the
 * supported RAM (2 GiB target: P = 524288; the production test asserts the
 * inequality at boot). Fragmentation can therefore never cause allocation
 * failure; allocation fails only at genuine resource exhaustion.
 *
 * Search is deterministic: one bounded pass over the live spans (two cursor
 * segments), independent of window size.
 */

#include "../include/kheap.h"
#include "../include/pmm.h"
#include "../include/uart.h"
#include "../include/arch/aarch64/mmu.h"

#define KHEAP_MAX_ALLOCATION_BYTES ((size_t)(128u * 1024u * 1024u))
#define KHEAP_MAGIC 0x4B484541u
#define KHEAP_SLAB_CLASS_COUNT 6u
#define KHEAP_SLAB_DESCRIPTOR_COUNT 65535u
#define KHEAP_NO_SLOT UINT16_MAX
#define KHEAP_VM_PAGE_COUNT (ZENIN_VM_HEAP_BYTES / PAGE_SIZE)
#define KHEAP_LARGE_MAX_PAGES (KHEAP_MAX_ALLOCATION_BYTES / PAGE_SIZE)

typedef struct kheap_span {
    struct kheap_span *prev;
    struct kheap_span *next;
    uint64_t start_page;
    uint32_t page_count;
    uint32_t descriptor_id; /* 0 for large-object headers; slab id otherwise. */
} kheap_span_t;

typedef struct kheap_vm_header {
    uint32_t magic;
    uint32_t requested;
    uint32_t page_count; /* Nonzero for a large VM allocation; zero for slab slots. */
    uint32_t allocated;
    kheap_span_t span;
} kheap_vm_header_t;

typedef struct kheap_slab_page {
    kheap_span_t span; /* One-page window span; start_page is the page index. */
    uint16_t class_index;
    uint16_t slot_count;
    uint16_t used_slots;
    uint16_t free_head;
    uint16_t available_previous; /* Descriptor IDs are index + 1; zero is null. */
    uint16_t available_next;
    uint16_t free_next;
    uint8_t active;
    uint8_t free_listed;
} kheap_slab_page_t;

_Static_assert(sizeof(kheap_vm_header_t) == KHEAP_VM_HEADER_BYTES,
               "VM heap header layout changed; update allocator metadata");
_Static_assert(sizeof(kheap_span_t) == 32u, "span node layout");
_Static_assert(KHEAP_LARGE_MAX_PAGES > 0 &&
               KHEAP_VM_PAGE_COUNT / KHEAP_LARGE_MAX_PAGES > 524288u,
               "D010 window guarantee requires floor(W/n_max) > max RAM pages");

static const uint16_t slab_class_bytes[KHEAP_SLAB_CLASS_COUNT] = {
    128u, 256u, 512u, 1024u, 2048u, 4096u
};
static const uint8_t slab_class_shift[KHEAP_SLAB_CLASS_COUNT] = {
    7u, 8u, 9u, 10u, 11u, 12u
};

static kheap_slab_page_t slab_pages[KHEAP_SLAB_DESCRIPTOR_COUNT];
static uint16_t slab_available_head[KHEAP_SLAB_CLASS_COUNT];
static uint16_t slab_empty_cached[KHEAP_SLAB_CLASS_COUNT];
static uint16_t slab_descriptor_free_head;
static uint32_t slab_descriptor_next_unused = 1;
static kheap_span_t *span_head; /* Sorted ascending by start_page. */
static size_t vm_page_search_hint;
static size_t vm_pages_in_use;
static size_t vm_live_allocations;
static size_t slab_pages_live;
static bool kheap_ready;

static void span_insert(kheap_span_t *node) {
    if (node->prev || node->next || span_head == node) {
        uart_puts("[zenin-kheap] PANIC: double span_insert\n");
        for (;;) __asm__ volatile("wfi");
    }
    for (kheap_span_t *n = span_head; n; n = n->next) {
        uint64_t a0 = node->start_page, a1 = a0 + node->page_count;
        uint64_t b0 = n->start_page, b1 = b0 + n->page_count;
        if (a0 < b1 && b0 < a1) {
            uart_puts("[zenin-kheap] PANIC: overlapping span_insert\n");
            for (;;) __asm__ volatile("wfi");
        }
    }
    kheap_span_t **link = &span_head;
    kheap_span_t *prev = NULL;
    while (*link && (*link)->start_page < node->start_page) {
        prev = *link;
        link = &(*link)->next;
    }
    node->prev = prev;
    node->next = *link;
    if (*link) (*link)->prev = node;
    *link = node;
}

static void span_remove(kheap_span_t *node) {
    if (span_head != node && !node->prev && !node->next) {
        uart_puts("[zenin-kheap] PANIC: orphan span_remove\n");
        for (;;) __asm__ volatile("wfi");
    }
    if (node->prev) node->prev->next = node->next;
    else span_head = node->next;
    if (node->next) node->next->prev = node->prev;
    node->prev = NULL;
    node->next = NULL;
}

static void vm_page_range_mark(size_t count) {
    vm_pages_in_use += count;
}

static void vm_page_release_count(size_t page, size_t count) {
    if (vm_pages_in_use >= count) vm_pages_in_use -= count;
    else vm_pages_in_use = 0;
    if (page < vm_page_search_hint) vm_page_search_hint = page;
}

/* Deterministic two-segment gap search over the live spans. The cursor scans
 * [hint, W) then [0, hint); a run never crosses a segment boundary. The
 * first undersized gap seen is preserved in the hint for later smaller
 * requests (D004). */
static size_t vm_find_free_run(size_t count) {
    if (count == 0 || count > KHEAP_VM_PAGE_COUNT - vm_pages_in_use) return SIZE_MAX;
    size_t earliest_small = SIZE_MAX;
    size_t found = SIZE_MAX;

    for (int pass = 0; pass < 2 && found == SIZE_MAX; ++pass) {
        size_t seg_lo = (pass == 0) ? vm_page_search_hint : 0;
        size_t seg_hi = (pass == 0) ? KHEAP_VM_PAGE_COUNT : vm_page_search_hint;
        if (seg_lo >= seg_hi) continue;
        size_t cursor = seg_lo;
        for (kheap_span_t *n = span_head; n != NULL; n = n->next) {
            size_t s = (size_t)n->start_page;
            size_t e = s + (size_t)n->page_count;
            if (e <= seg_lo || s >= seg_hi) continue;
            if (s > cursor) {
                size_t gap_hi = (s < seg_hi) ? s : seg_hi;
                if (gap_hi - cursor >= count) {
                    found = cursor;
                    break;
                }
                if (earliest_small == SIZE_MAX) earliest_small = cursor;
            }
            if (e > cursor) cursor = e;
        }
        if (found == SIZE_MAX && cursor < seg_hi) {
            if (seg_hi - cursor >= count) {
                found = cursor;
            } else if (earliest_small == SIZE_MAX) {
                earliest_small = cursor;
            }
        }
    }

    if (found == SIZE_MAX) return SIZE_MAX;
    if (earliest_small != SIZE_MAX) {
        /* Keep the first undersized gap searchable for smaller requests. */
        vm_page_search_hint = earliest_small;
    } else {
        vm_page_search_hint = found + count;
        if (vm_page_search_hint == KHEAP_VM_PAGE_COUNT) vm_page_search_hint = 0;
    }
    return found;
}

static size_t vm_largest_free_run_pages(void) {
    size_t largest = 0;
    size_t cursor = 0;
    for (kheap_span_t *n = span_head; n != NULL; n = n->next) {
        size_t s = (size_t)n->start_page;
        if (s > cursor && s - cursor > largest) largest = s - cursor;
        size_t e = s + (size_t)n->page_count;
        if (e > cursor) cursor = e;
    }
    if (KHEAP_VM_PAGE_COUNT > cursor && KHEAP_VM_PAGE_COUNT - cursor > largest)
        largest = KHEAP_VM_PAGE_COUNT - cursor;
    return largest;
}

static void slab_available_insert(uint16_t id) {
    kheap_slab_page_t *page = &slab_pages[id - 1u];
    uint16_t old_head = slab_available_head[page->class_index];
    page->available_previous = 0;
    page->available_next = old_head;
    if (old_head) slab_pages[old_head - 1u].available_previous = id;
    slab_available_head[page->class_index] = id;
}

static void slab_available_remove(uint16_t id) {
    kheap_slab_page_t *page = &slab_pages[id - 1u];
    if (page->available_previous)
        slab_pages[page->available_previous - 1u].available_next = page->available_next;
    else
        slab_available_head[page->class_index] = page->available_next;
    if (page->available_next)
        slab_pages[page->available_next - 1u].available_previous = page->available_previous;
    page->available_previous = 0;
    page->available_next = 0;
}

static uint16_t slab_descriptor_take(void) {
    uint16_t id = slab_descriptor_free_head;
    if (id) {
        kheap_slab_page_t *page = &slab_pages[id - 1u];
        if (page->active || !page->free_listed) {
            uart_puts("[zenin-kheap] PANIC: descriptor double-take\n");
            for (;;) __asm__ volatile("wfi");
        }
        slab_descriptor_free_head = page->free_next;
        page->free_next = 0;
        page->free_listed = 0;
        page->active = 0;
    } else {
        if (slab_descriptor_next_unused > KHEAP_SLAB_DESCRIPTOR_COUNT) return 0;
        id = (uint16_t)slab_descriptor_next_unused++;
    }
    return id;
}

static void slab_descriptor_return(uint16_t id) {
    kheap_slab_page_t *page = &slab_pages[id - 1u];
    if (page->free_listed) {
        uart_puts("[zenin-kheap] PANIC: descriptor double-return\n");
        for (;;) __asm__ volatile("wfi");
    }
    page->active = 0;
    page->free_listed = 1;
    page->span.start_page = 0;
    page->span.page_count = 0;
    page->span.descriptor_id = 0;
    page->class_index = 0;
    page->slot_count = 0;
    page->used_slots = 0;
    page->free_head = KHEAP_NO_SLOT;
    page->available_previous = 0;
    page->available_next = 0;
    page->free_next = slab_descriptor_free_head;
    slab_descriptor_free_head = id;
}

void kheap_init(void) {
    if (kheap_ready) return;
    slab_descriptor_free_head = 0;
    slab_descriptor_next_unused = 1;
    span_head = NULL;
    kheap_ready = true;
}

static bool vm_map_fresh_page(uintptr_t va, uintptr_t *physical_out) {
    void *physical = pmm_alloc_page();
    if (!physical) return false;
    if (!arch_vm_map_heap_page(va, (uintptr_t)physical)) {
        pmm_free_page(physical);
        return false;
    }
    if (physical_out) *physical_out = (uintptr_t)physical;
    return true;
}

static void vm_rollback_pages(uintptr_t start, size_t mapped_pages) {
    for (size_t i = 0; i < mapped_pages; ++i) {
        uintptr_t physical;
        if (arch_vm_unmap_heap_page(start + i * PAGE_SIZE, &physical))
            pmm_free_page((void *)physical);
    }
}

static void *vm_large_kmalloc(size_t size) {
    size_t total = size + sizeof(kheap_vm_header_t);
    size_t pages = (total + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (!pages || pages > KHEAP_LARGE_MAX_PAGES) return NULL;
    size_t saved_hint = vm_page_search_hint;
    size_t start_page = vm_find_free_run(pages);
    if (start_page == SIZE_MAX) return NULL;
    vm_page_range_mark(pages);
    uintptr_t start = ZENIN_VM_HEAP_BASE + start_page * PAGE_SIZE;
    size_t mapped = 0;
    for (; mapped < pages; ++mapped) {
        if (!vm_map_fresh_page(start + mapped * PAGE_SIZE, NULL)) {
            vm_rollback_pages(start, mapped);
            vm_pages_in_use -= pages;
            vm_page_search_hint = saved_hint;
            return NULL;
        }
    }

    kheap_vm_header_t *header = (kheap_vm_header_t *)start;
    header->magic = KHEAP_MAGIC;
    header->requested = (uint32_t)size;
    header->page_count = (uint32_t)pages;
    header->allocated = 1;
    header->span.prev = NULL;
    header->span.next = NULL;
    header->span.start_page = (uint64_t)start_page;
    header->span.page_count = (uint32_t)pages;
    header->span.descriptor_id = 0;
    span_insert(&header->span);
    vm_live_allocations++;
    return (void *)(start + sizeof(*header));
}

static int slab_class_for_size(size_t total_bytes) {
    for (uint32_t i = 0; i < KHEAP_SLAB_CLASS_COUNT; ++i)
        if (total_bytes <= slab_class_bytes[i]) return (int)i;
    return -1;
}

static uint16_t slab_page_create(uint16_t class_index) {
    uint16_t id = slab_descriptor_take();
    if (!id) return 0;
    size_t saved_hint = vm_page_search_hint;
    size_t page_index = vm_find_free_run(1);
    if (page_index == SIZE_MAX) {
        slab_descriptor_return(id);
        return 0;
    }

    vm_page_range_mark(1);
    uintptr_t va = ZENIN_VM_HEAP_BASE + page_index * PAGE_SIZE;
    if (!vm_map_fresh_page(va, NULL)) {
        vm_page_search_hint = saved_hint;
        vm_pages_in_use -= 1;
        slab_descriptor_return(id);
        return 0;
    }

    kheap_slab_page_t *page = &slab_pages[id - 1u];
    uint16_t class_bytes = slab_class_bytes[class_index];
    uint16_t slots = (uint16_t)(PAGE_SIZE / class_bytes);
    page->span.prev = NULL;
    page->span.next = NULL;
    page->span.start_page = (uint64_t)page_index;
    page->span.page_count = 1;
    page->span.descriptor_id = id;
    page->class_index = class_index;
    page->slot_count = slots;
    page->used_slots = 0;
    page->free_head = 0;
    page->available_previous = 0;
    page->available_next = 0;
    page->active = 1;
    for (uint16_t slot = 0; slot < slots; ++slot) {
        uint16_t next = (slot + 1u < slots) ? (uint16_t)(slot + 1u) : KHEAP_NO_SLOT;
        *(uint16_t *)(va + (size_t)slot * class_bytes) = next;
    }
    span_insert(&page->span);
    slab_pages_live++;
    slab_available_insert(id);
    return id;
}

static void *slab_kmalloc(size_t size, uint16_t class_index) {
    uint16_t id = slab_available_head[class_index];
    if (!id) id = slab_page_create(class_index);
    if (!id) return NULL;

    kheap_slab_page_t *page = &slab_pages[id - 1u];
    uint16_t slot = page->free_head;
    uintptr_t slot_address = ZENIN_VM_HEAP_BASE +
                             (uintptr_t)page->span.start_page * PAGE_SIZE +
                             (size_t)slot * slab_class_bytes[class_index];
    page->free_head = *(uint16_t *)slot_address;
    page->used_slots++;
    if (page->used_slots == page->slot_count) slab_available_remove(id);
    if (slab_empty_cached[class_index] == id) slab_empty_cached[class_index] = 0;

    kheap_vm_header_t *header = (kheap_vm_header_t *)slot_address;
    header->magic = KHEAP_MAGIC;
    header->requested = (uint32_t)size;
    header->page_count = 0;
    header->allocated = 1;
    header->span.prev = NULL;
    header->span.next = NULL;
    header->span.start_page = slot;
    header->span.page_count = 0;
    header->span.descriptor_id = id;
    vm_live_allocations++;
    return (void *)(slot_address + sizeof(*header));
}

static bool slab_page_release(uint16_t id) {
    kheap_slab_page_t *page = &slab_pages[id - 1u];
    if (!page->active || page->used_slots != 0) return false;
    size_t page_index = (size_t)page->span.start_page;
    uintptr_t va = ZENIN_VM_HEAP_BASE + page_index * PAGE_SIZE;
    uintptr_t physical;
    if (!arch_vm_unmap_heap_page(va, &physical)) return false;
    slab_available_remove(id);
    if (slab_empty_cached[page->class_index] == id)
        slab_empty_cached[page->class_index] = 0;
    span_remove(&page->span);
    vm_page_release_count(page_index, 1);
    pmm_free_page((void *)physical);
    if (slab_pages_live) slab_pages_live--;
    slab_descriptor_return(id);
    return true;
}

static void vm_release_empty_caches(void) {
    for (uint16_t class_index = 0; class_index < KHEAP_SLAB_CLASS_COUNT; ++class_index) {
        uint16_t id = slab_empty_cached[class_index];
        if (id) (void)slab_page_release(id);
    }
}

static void vm_reset_if_empty(void) {
    if (vm_live_allocations != 0) return;
    vm_release_empty_caches();
    if (vm_pages_in_use == 0) vm_page_search_hint = 0;
}

static bool slab_kfree(uintptr_t payload, uintptr_t header_address, uint32_t id) {
    if (!id || id > KHEAP_SLAB_DESCRIPTOR_COUNT) return false;
    kheap_slab_page_t *page = &slab_pages[id - 1u];
    if (!page->active || page->used_slots == 0) return false;
    uintptr_t page_address = ZENIN_VM_HEAP_BASE +
                             (uintptr_t)page->span.start_page * PAGE_SIZE;
    uintptr_t page_offset = header_address - page_address;
    uint16_t class_bytes = slab_class_bytes[page->class_index];
    if (payload != header_address + sizeof(kheap_vm_header_t) ||
        page_offset >= PAGE_SIZE || (page_offset & (class_bytes - 1u)) != 0) return false;
    uint16_t slot = (uint16_t)(page_offset >>
                               slab_class_shift[page->class_index]);
    if (slot >= page->slot_count) return false;

    kheap_vm_header_t *header = (kheap_vm_header_t *)header_address;
    if (header->magic != KHEAP_MAGIC || !header->allocated || header->page_count != 0 ||
        header->span.descriptor_id != id || header->span.start_page != slot) return false;

    bool was_full = page->used_slots == page->slot_count;
    header->allocated = 0;
    header->magic = 0;
    *(uint16_t *)header_address = page->free_head;
    page->free_head = slot;
    page->used_slots--;
    if (vm_live_allocations) vm_live_allocations--;
    if (was_full) slab_available_insert(id);

    if (page->used_slots == 0) {
        if (slab_empty_cached[page->class_index] == 0) {
            slab_empty_cached[page->class_index] = id;
        } else if (slab_empty_cached[page->class_index] != id) {
            (void)slab_page_release(id);
        }
    }
    vm_reset_if_empty();
    return true;
}

void *kmalloc(size_t size) {
    if (size == 0 || size > KHEAP_MAX_ALLOCATION_BYTES - KHEAP_VM_HEADER_BYTES ||
        size > SIZE_MAX - KHEAP_VM_HEADER_BYTES) return NULL;
    if (!kheap_ready) kheap_init();

    size_t total = size + KHEAP_VM_HEADER_BYTES;
    int class_index = slab_class_for_size(total);
    if (class_index >= 0) return slab_kmalloc(size, (uint16_t)class_index);
    return vm_large_kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;
    uintptr_t payload = (uintptr_t)ptr;
    if (payload < ZENIN_VM_HEAP_BASE + sizeof(kheap_vm_header_t) ||
        payload >= ZENIN_VM_HEAP_BASE + ZENIN_VM_HEAP_BYTES) return;

    uintptr_t header_address = payload - sizeof(kheap_vm_header_t);
    /* Validate the page is mapped before touching header memory; this makes
     * interior, foreign, and use-after-free pointers fault-free rejects. */
    uintptr_t probe;
    if (!arch_vm_query_heap_page(header_address & ~(uintptr_t)(PAGE_SIZE - 1u), &probe))
        return;
    kheap_vm_header_t *header = (kheap_vm_header_t *)header_address;
    if (header->page_count == 0) {
        (void)slab_kfree(payload, header_address, header->span.descriptor_id);
        return;
    }

    if ((header_address & (PAGE_SIZE - 1u)) != 0 ||
        header->span.start_page !=
            (uint64_t)((header_address - ZENIN_VM_HEAP_BASE) / PAGE_SIZE)) return;
    if (header->magic != KHEAP_MAGIC || !header->allocated ||
        header->requested > KHEAP_MAX_ALLOCATION_BYTES - KHEAP_VM_HEADER_BYTES) return;
    size_t expected_pages = (header->requested + sizeof(*header) + PAGE_SIZE - 1u) / PAGE_SIZE;
    if (header->page_count != expected_pages ||
        expected_pages > KHEAP_LARGE_MAX_PAGES) return;

    size_t page_count = header->page_count;
    size_t start_page = (size_t)header->span.start_page;
    header->allocated = 0;
    header->magic = 0;
    span_remove(&header->span);
    for (size_t i = 0; i < page_count; ++i) {
        uintptr_t physical;
        if (!arch_vm_unmap_heap_page(header_address + i * PAGE_SIZE, &physical)) return;
        pmm_free_page((void *)physical);
    }
    vm_page_release_count(start_page, page_count);
    if (vm_live_allocations) vm_live_allocations--;
    vm_reset_if_empty();
}

/* Derived accounting (D011): walks the live span list; equals the actual
 * live allocation state by construction. */
size_t kheap_get_allocated_bytes(void) {
    size_t total = 0;
    size_t walked = 0;
    for (kheap_span_t *n = span_head; n; n = n->next) {
        if (++walked > 4096u) {
            uart_puts("[zenin-kheap] PANIC: span list cycle in accounting walk\n");
            for (;;) __asm__ volatile("wfi");
        }
        if (n->descriptor_id == 0) {
            const kheap_vm_header_t *h =
                (const kheap_vm_header_t *)((const uint8_t *)n -
                                            offsetof(kheap_vm_header_t, span));
            total += h->requested;
        } else {
            const kheap_slab_page_t *page = &slab_pages[n->descriptor_id - 1u];
            size_t class_bytes = slab_class_bytes[page->class_index];
            const uint8_t *base = (const uint8_t *)(ZENIN_VM_HEAP_BASE +
                                                   n->start_page * PAGE_SIZE);
            for (uint32_t s = 0; s < page->slot_count; ++s) {
                const kheap_vm_header_t *h =
                    (const kheap_vm_header_t *)(base + (size_t)s * class_bytes);
                if (h->magic == KHEAP_MAGIC && h->allocated) total += h->requested;
            }
        }
    }
    return total;
}

size_t kheap_get_largest_free_block(void) {
    size_t free_pages = pmm_get_free_pages();
    if (free_pages <= 1) return 0;
    size_t max_by_physical = (free_pages - 1u) * PAGE_SIZE - KHEAP_VM_HEADER_BYTES;
    size_t virtual_remaining = vm_largest_free_run_pages() * PAGE_SIZE;
    size_t max_by_virtual = virtual_remaining > KHEAP_VM_HEADER_BYTES
                                ? virtual_remaining - KHEAP_VM_HEADER_BYTES
                                : 0;
    size_t max = KHEAP_MAX_ALLOCATION_BYTES - KHEAP_VM_HEADER_BYTES;
    if (max > max_by_physical) max = max_by_physical;
    if (max > max_by_virtual) max = max_by_virtual;
    return max;
}

size_t kheap_get_arena_bytes(void) {
    return KHEAP_MAX_ALLOCATION_BYTES;
}
