#ifndef ZENIN_ARCH_AARCH64_MMU_H
#define ZENIN_ARCH_AARCH64_MMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Heap virtual window (D010/D012): 96 TiB of 48-bit VA starting at its own
 * L0 table slot (0x800000000). Every allocated heap VA page is backed 1:1
 * by a physical page, so at most P pages are ever allocated in this window.
 * With W = 96 TiB and a 128 MiB per-allocation cap (n_max = 32768 pages),
 * blocking every n_max-page free run would need floor(W/n_max) = 786432
 * allocated pages, which exceeds any supported RAM (the boot-time guarantee
 * check covers up to ~3 GiB RAM). Fragmentation can therefore never cause
 * allocation failure for requests <= 128 MiB.
 * D012 lock: every table descriptor in the frozen root that points into
 * this subtree carries PXNTable|UXNTable, so no heap page is ever
 * executable even if a lower-level heap table were corrupted. */
#define ZENIN_VM_HEAP_BASE ((uintptr_t)0x8000000000ULL)
#define ZENIN_VM_HEAP_BYTES ((size_t)(96ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL))
#define ZENIN_VM_PAGE_SIZE ((size_t)4096u)

#include "../../boot_info.h"
void arch_mmu_enable(const zenin_boot_info_t *boot_info);
bool arch_mmu_identity_map_installed(void);
bool arch_mmu_device_window_verified(void);
bool arch_vm_map_heap_page(uintptr_t virtual_address, uintptr_t physical_address);
bool arch_vm_query_heap_page(uintptr_t virtual_address, uintptr_t *physical_address);
bool arch_vm_unmap_heap_page(uintptr_t virtual_address, uintptr_t *physical_address);

/* D012: W^X verification and split-subtree freeze. */
bool arch_vm_query_permissions(uintptr_t virtual_address, bool *writable, bool *executable);
bool arch_mmu_heap_subtree_xn_locked(void);
uintptr_t arch_mmu_root_table_address(void);
void arch_mmu_freeze_kernel_map(void);

#endif
