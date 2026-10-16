/*
 * Project Zenin - Dynamic Kernel Heap Allocator (kmalloc / kfree)
 * Fixed-time O(1) Slab / Block Allocator.
 * Designed for Deterministic Latency, Zero Fragmentation, and Zero Frame Micro-stutter.
 */

#ifndef ZENIN_KHEAP_H
#define ZENIN_KHEAP_H

#include <stdint.h>
#include <stddef.h>

/* Initialize kernel heap using pages from PMM */
void kheap_init(void);

/* Allocate size bytes from the kernel dynamic heap */
void *kmalloc(size_t size);

/* Free dynamic memory back to the heap */
void kfree(void *ptr);

/* Query heap memory metrics */
size_t kheap_get_allocated_bytes(void);

#endif /* ZENIN_KHEAP_H */
