/* Project Zenin - page-mapped size-class and large-object kernel allocator. */

#ifndef ZENIN_KHEAP_H
#define ZENIN_KHEAP_H

#include <stdint.h>
#include <stddef.h>

/* Per-allocation metadata size (payload pointer = allocation base + this). */
#define KHEAP_VM_HEADER_BYTES 48u

/* Initialize slab and page-mapped allocator metadata; backing is acquired lazily. */
void kheap_init(void);

/* Allocate size bytes from the kernel dynamic heap */
void *kmalloc(size_t size);

/* Free dynamic memory back to the heap */
void kfree(void *ptr);

/* Query byte accounting, a free-capacity estimate, and the per-allocation size limit. */
size_t kheap_get_allocated_bytes(void);
size_t kheap_get_largest_free_block(void);
size_t kheap_get_arena_bytes(void);

#endif /* ZENIN_KHEAP_H */
