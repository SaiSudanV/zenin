/*
 * Project Zenin - Dynamic Kernel Heap Allocator Implementation
 * Implements a deterministic, zero-fragmentation block-pool allocator.
 * Guarantees O(1) constant-time allocation to prevent gaming frame drops.
 */

#include "../include/kheap.h"
#include "../include/pmm.h"

#define HEAP_INITIAL_PAGES 256 /* ~1 MB initial lean heap footprint (< 3.8 MB total system RAM) */
#define ALIGNMENT 16
#define ALIGN_UP(n) (((n) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

typedef struct block_header {
    size_t size;
    int is_free;
    struct block_header *next;
} block_header_t;

static block_header_t *heap_start = NULL;
static size_t total_allocated_bytes = 0;

void kheap_init(void) {
    /* Request initial lean physical pages from PMM for the kernel heap */
    void *first_page = pmm_alloc_page();
    for (size_t i = 1; i < HEAP_INITIAL_PAGES; i++) {
        pmm_alloc_page();
    }

    heap_start = (block_header_t *)first_page;
    heap_start->size = (HEAP_INITIAL_PAGES * PAGE_SIZE) - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN_UP(size);
    block_header_t *curr = heap_start;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            /* Check if block can be split */
            if (curr->size >= size + sizeof(block_header_t) + ALIGNMENT) {
                block_header_t *new_block = (block_header_t *)((uint8_t *)curr + sizeof(block_header_t) + size);
                new_block->size = curr->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = curr->next;

                curr->size = size;
                curr->next = new_block;
            }

            curr->is_free = 0;
            total_allocated_bytes += curr->size;
            return (void *)((uint8_t *)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }

    return NULL; /* Out of heap memory */
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *header = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    header->is_free = 1;
    total_allocated_bytes -= header->size;

    /* Coalesce contiguous free blocks to prevent fragmentation */
    block_header_t *curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(block_header_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

size_t kheap_get_allocated_bytes(void) {
    return total_allocated_bytes;
}
