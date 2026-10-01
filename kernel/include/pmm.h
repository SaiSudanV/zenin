/*
 * Project Zenin - Physical Memory Manager (PMM)
 * 4KB Page Frame Allocator using a high-speed bitset.
 * RAM Overhead: Exactly 1 bit per 4KB page (Only 64 KB to manage 2 GB of RAM!)
 */

#ifndef ZENIN_PMM_H
#define ZENIN_PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096 /* Standard 4KB Page Frame */

/* Initialize the physical memory manager with total RAM available */
void pmm_init(uintptr_t mem_start, size_t mem_size);

/* Allocate a single 4KB physical page frame */
void *pmm_alloc_page(void);

/* Free a previously allocated 4KB page frame */
void pmm_free_page(void *ptr);

/* Query available physical memory statistics */
size_t pmm_get_free_pages(void);
size_t pmm_get_total_pages(void);

#endif /* ZENIN_PMM_H */
