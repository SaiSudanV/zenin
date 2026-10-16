/* Relocatable heap objects addressed by generation-checked handles. */
#ifndef ZENIN_KHEAP_HANDLE_H
#define ZENIN_KHEAP_HANDLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t kheap_handle_t;
typedef void (*kheap_handle_access_fn)(void *bytes, size_t length, void *context);

#define KHEAP_HANDLE_NULL ((kheap_handle_t)0)

/* The arena is acquired from PMM on first use and retained until shutdown. */
bool kheap_handle_alloc(size_t size, kheap_handle_t *out_handle);
bool kheap_handle_free(kheap_handle_t handle);
size_t kheap_handle_size(kheap_handle_t handle);

/*
 * The pointer is valid only during the callback. Alloc/free operations from
 * inside a callback are rejected so a callback cannot trigger relocation.
 */
bool kheap_handle_access(kheap_handle_t handle,
                         kheap_handle_access_fn callback,
                         void *context);

/* Metrics for the fixed-capacity compacting arena. */
size_t kheap_handle_capacity(void);
size_t kheap_handle_max_objects(void);
size_t kheap_handle_live_bytes(void);

#endif
