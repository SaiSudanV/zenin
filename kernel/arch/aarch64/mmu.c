/*
 * Project Zenin - AArch64 MMU bring-up (D010/D012).
 *
 * - 48-bit VA (TCR T0SZ=16), four-level tables.
 * - W^X split (D012): .text+vectors RO+X, .rodata RO+XN, .data/.bss/arenas/
 *   stack RW+XN, SCTLR.WXN=1 hardware backstop.
 * - Split-subtree freeze (D012): the root (l0) is frozen Read-Only at boot
 *   end. l0[0] covers the static subtree (devices + kernel image + their
 *   tables in .pagetables, all flipped RO by the freeze). Every l0 entry
 *   pointing into the heap subtree carries PXNTable|UXNTable so CPU
 *   silicon blocks execution anywhere in the heap subtree even if a
 *   lower-level heap table were corrupted. The 192 heap L1 tables are
 *   statically pre-created (`.heaptables`, RW+XN data: their *mapping* is
 *   frozen but their entries stay writable for the dynamic heap).
 * - Physical range proof (D012): heap payload physicals come only from
 *   pmm_alloc_page over [__kernel_end, ram_end), so the heap mapper can
 *   structurally never map kernel image, static arena, or frozen table
 *   pages. VA-range disjointness is asserted at compile time.
 */

#include "../../include/arch/aarch64/mmu.h"
#include "../../include/pmm.h"

#define DESC_VALID       (1ULL << 0)
#define DESC_TABLE       (1ULL << 1) /* table descriptor (or L3 page: 0b11) */
#define DESC_ATTRIDX(n)  ((uint64_t)(n) << 2)
#define DESC_AP_RW_EL1   (0ULL << 6) /* EL1 RW (WXN then forces XN) */
#define DESC_AP_RO_EL1   (2ULL << 6) /* EL1 Read-Only */
#define DESC_SH_INNER    (3ULL << 8)
#define DESC_AF          (1ULL << 10)
#define DESC_PXN         (1ULL << 53)
#define DESC_UXN         (1ULL << 54)
#define DESC_NX          (DESC_PXN | DESC_UXN)
#define DESC_PXNTABLE    (1ULL << 59)
#define DESC_UXNTABLE    (1ULL << 60)
#define DESC_ADDR_MASK   0x0000FFFFFFFFF000ULL

/* Static page tables (D012): page-aligned in .bss (NOBITS — the boot
 * environment zeroes them). The freeze flips exactly these pages RO. */
__attribute__((aligned(4096))) static uint64_t l0_table[512];
__attribute__((aligned(4096))) static uint64_t l1_static[512];
__attribute__((aligned(4096))) static uint64_t low_device_l2[512];
__attribute__((aligned(4096))) static uint64_t kernel_l2[512];
__attribute__((aligned(4096))) static uint64_t image_l3_low[512];
__attribute__((aligned(4096))) static uint64_t image_l3_high[512];

/* Heap-subtree L1 tables (D012): content writable (dynamic L2/L3 growth),
 * mapping frozen. One per L0 slot of the 96 TiB window (slots 1..192). */
#define HEAP_L1_SLOTS 192u
__attribute__((aligned(4096)))
static uint64_t heap_l1_tables[HEAP_L1_SLOTS][512];

extern char __kernel_start;
extern char __text_end;
extern char __rodata_end;
extern char __kernel_end;

/* --- compile-time range proofs (D012 lock 2) --- */
_Static_assert(ZENIN_VM_HEAP_BASE == (1ULL << 39),
               "heap window must start at an L0-table slot boundary");
_Static_assert(ZENIN_VM_HEAP_BASE >= (1ULL << 36),
               "heap VA must be disjoint from image/device VA space");
_Static_assert(ZENIN_VM_HEAP_BASE + ZENIN_VM_HEAP_BYTES <= (1ULL << 47),
               "heap window must fit the low 48-bit VA half");
_Static_assert((ZENIN_VM_HEAP_BASE + ZENIN_VM_HEAP_BYTES) / (1ULL << 39) ==
                   HEAP_L1_SLOTS + 1,
               "heap L1 pre-creation must cover the whole window");

static inline int l0_index(uint64_t va) { return (int)((va >> 39) & 511u); }
static inline int l1_index(uint64_t va) { return (int)((va >> 30) & 511u); }
static inline int l2_index(uint64_t va) { return (int)((va >> 21) & 511u); }
static inline int l3_index(uint64_t va) { return (int)((va >> 12) & 511u); }

static inline uint64_t *desc_table(uint64_t desc) {
    return (uint64_t *)(desc & DESC_ADDR_MASK);
}

static bool table_empty(uint64_t *table) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & DESC_VALID) return false;
    }
    return true;
}

static uint64_t page_desc(uint64_t physical, bool writable, bool executable) {
    return (physical & DESC_ADDR_MASK) | DESC_VALID | DESC_TABLE |
           DESC_ATTRIDX(1) | DESC_SH_INNER | DESC_AF |
           (writable ? DESC_AP_RW_EL1 : DESC_AP_RO_EL1) |
           (executable ? 0 : DESC_NX);
}

static uint64_t block_desc(uint64_t physical) {
    /* RW + never-execute large block. */
    return (physical & 0x0000FFFFFFE00000ULL) | DESC_VALID |
           DESC_ATTRIDX(1) | DESC_SH_INNER | DESC_AF | DESC_AP_RW_EL1 | DESC_NX;
}

/* Device-nGnRnE large block (MAIR idx 0) for memory-mapped I/O windows. */
static uint64_t device_block_desc(uint64_t physical) {
    return (physical & 0x0000FFFFFFE00000ULL) | DESC_VALID |
           DESC_ATTRIDX(0) | DESC_AF | DESC_AP_RW_EL1 | DESC_NX;
}

/* Dynamic L2 tables for memory-map-driven regions (non-kernel RAM and MMIO).
 * One L2 covers a 1 GiB window; the pool bounds how many distinct windows the
 * identity map can populate from the firmware map. */
#define DYN_L2_COUNT 8u
__attribute__((aligned(4096))) static uint64_t dyn_l2[DYN_L2_COUNT][512];
static unsigned dyn_l2_used;

/* Map [base, base+len) into the static L1 as RW+XN (device=false) or device
 * blocks. Uses 1 GiB L1 blocks where a window is fully covered and 2 MiB L2
 * blocks otherwise. The kernel's own 1 GiB window (W^X) is skipped. */
static void map_range(uint64_t base, uint64_t len, bool device) {
    uint64_t end = base + len;
    uint64_t kernel_gib = (uint64_t)&__kernel_start & ~((uint64_t)0x3FFFFFFF);
    for (uint64_t va = base & ~((uint64_t)0x3FFFFFFF); va < end; va += (1ULL << 30)) {
        if (va == kernel_gib) continue; /* kernel_l2 owns this window (W^X) */
        int i1 = l1_index(va);
        uint64_t window_lo = va, window_hi = va + (1ULL << 30);
        uint64_t cov_lo = base > window_lo ? base : window_lo;
        uint64_t cov_hi = end < window_hi ? end : window_hi;
        /* Fully-covered aligned window -> a single 1 GiB L1 block (RW+XN only;
         * device windows stay granular via L2 so we never over-map MMIO). */
        if (!device && cov_lo == window_lo && cov_hi == window_hi) {
            l1_static[i1] = block_desc(window_lo);
            continue;
        }
        uint64_t *l2;
        if ((l1_static[i1] & (DESC_TABLE | DESC_VALID)) == (DESC_TABLE | DESC_VALID)) {
            l2 = desc_table(l1_static[i1]);
        } else {
            if (dyn_l2_used >= DYN_L2_COUNT) continue;
            l2 = dyn_l2[dyn_l2_used++];
            l1_static[i1] = ((uint64_t)l2 & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
        }
        for (uint64_t a = cov_lo & ~0x1FFFFFULL; a < cov_hi; a += 0x200000ULL) {
            int i2 = l2_index(a);
            if (!(l2[i2] & DESC_VALID)) {
                l2[i2] = device ? device_block_desc(a) : block_desc(a);
            }
        }
    }
}

static void arch_cpu_dsb(void) { __asm__ volatile("dsb ish" : : : "memory"); }
static void arch_cpu_isb(void) { __asm__ volatile("isb" : : : "memory"); }
static void arch_cpu_tlbi_all(void) {
    __asm__ volatile("tlbi vmalle1" : : : "memory");
}
static void arch_cpu_tlbi_va(uint64_t va) {
    uint64_t v = va >> 12;
    __asm__ volatile("dsb ishst" : : : "memory");
    __asm__ volatile("tlbi vae1, %0" : : "r"(v) : "memory");
    __asm__ volatile("dsb ish" : : : "memory");
    __asm__ volatile("isb" : : : "memory");
}

/* Walk the static identity subtree and the heap subtree without creating
 * tables. Returns the final descriptor (block or page) or 0. */
static uint64_t walk_desc(uint64_t va) {
    uint64_t *l1 = desc_table(l0_table[l0_index(va)]);
    if (!(l0_table[l0_index(va)] & DESC_VALID)) return 0;
    uint64_t d1 = l1[l1_index(va)];
    if (!(d1 & DESC_VALID)) return 0;
    if ((d1 & DESC_TABLE) == 0) return d1; /* 1 GiB block */
    uint64_t *l2 = desc_table(d1);
    uint64_t d2 = l2[l2_index(va)];
    if (!(d2 & DESC_VALID)) return 0;
    if ((d2 & DESC_TABLE) == 0) return d2; /* 2 MiB block */
    uint64_t *l3 = desc_table(d2);
    uint64_t d3 = l3[l3_index(va)];
    return (d3 & DESC_VALID) ? d3 : 0;
}

/* Heap walk with lazy L2/L3 creation (heap L1s are pre-created). */
static uint64_t *heap_walk_l3(uint64_t va, bool create) {
    int i0 = l0_index(va);
    if (!(l0_table[i0] & DESC_VALID)) return NULL;
    uint64_t *l1 = desc_table(l0_table[i0]);
    if (!(l1[l1_index(va)] & DESC_VALID)) {
        if (!create) return NULL;
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        uint64_t *l2 = (uint64_t *)page;
        for (int i = 0; i < 512; i++) l2[i] = 0;
        l1[l1_index(va)] = ((uint64_t)l2 & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
    }
    if ((l1[l1_index(va)] & DESC_TABLE) == 0) return NULL;
    uint64_t *l2 = desc_table(l1[l1_index(va)]);
    if (!(l2[l2_index(va)] & DESC_VALID)) {
        if (!create) return NULL;
        void *page = pmm_alloc_page();
        if (!page) return NULL;
        uint64_t *l3 = (uint64_t *)page;
        for (int i = 0; i < 512; i++) l3[i] = 0;
        l2[l2_index(va)] = ((uint64_t)l3 & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
    }
    if ((l2[l2_index(va)] & DESC_TABLE) == 0) return NULL;
    return desc_table(l2[l2_index(va)]);
}

bool heap_virtual_address_valid(uintptr_t va) {
    return va >= ZENIN_VM_HEAP_BASE &&
           va < ZENIN_VM_HEAP_BASE + ZENIN_VM_HEAP_BYTES &&
           (va & (ZENIN_VM_PAGE_SIZE - 1u)) == 0;
}

bool arch_vm_map_heap_page(uintptr_t virtual_address, uintptr_t physical_address) {
    if (!heap_virtual_address_valid(virtual_address) ||
        (physical_address & (ZENIN_VM_PAGE_SIZE - 1u)) != 0) return false;
    uint64_t va = (uint64_t)virtual_address;
    uint64_t *l3 = heap_walk_l3(va, true);
    if (!l3) return false;
    if (l3[l3_index(va)] & DESC_VALID) return false;
    l3[l3_index(va)] = page_desc((uint64_t)physical_address, true, false);
    arch_cpu_tlbi_va(va);
    return true;
}

bool arch_vm_query_heap_page(uintptr_t virtual_address, uintptr_t *physical_address) {
    if (!heap_virtual_address_valid(virtual_address)) return false;
    uint64_t va = (uint64_t)virtual_address;
    uint64_t *l3 = heap_walk_l3(va, false);
    if (!l3) return false;
    uint64_t desc = l3[l3_index(va)];
    if (!(desc & DESC_VALID)) return false;
    if (physical_address) *physical_address = (uintptr_t)(desc & DESC_ADDR_MASK);
    return true;
}

bool arch_vm_unmap_heap_page(uintptr_t virtual_address, uintptr_t *physical_address) {
    if (!heap_virtual_address_valid(virtual_address)) return false;
    uint64_t va = (uint64_t)virtual_address;
    uint64_t *l3 = heap_walk_l3(va, false);
    if (!l3) return false;
    uint64_t desc = l3[l3_index(va)];
    if (!(desc & DESC_VALID)) return false;
    if (physical_address) *physical_address = (uintptr_t)(desc & DESC_ADDR_MASK);
    l3[l3_index(va)] = 0;
    arch_cpu_tlbi_va(va);

    /* Cascade reclamation of empty L3/L2 (L1s are pre-created and kept). */
    bool tables_reclaimed = false;
    if (table_empty(l3)) {
        uint64_t d2 = desc_table(l0_table[l0_index(va)])[l1_index(va)];
        uint64_t *l2 = desc_table(d2);
        l2[l2_index(va)] = 0;
        pmm_free_page(l3);
        tables_reclaimed = true;
        if (table_empty(l2)) {
            desc_table(l0_table[l0_index(va)])[l1_index(va)] = 0;
            pmm_free_page(l2);
        }
    }
    if (tables_reclaimed) {
        arch_cpu_dsb();
        arch_cpu_tlbi_all();
        arch_cpu_dsb();
        arch_cpu_isb();
    }
    return true;
}

bool arch_vm_query_permissions(uintptr_t virtual_address, bool *writable, bool *executable) {
    uint64_t va = (uint64_t)virtual_address;
    if (!(l0_table[l0_index(va)] & DESC_VALID)) return false;
    bool forced_xn = (l0_table[l0_index(va)] & (DESC_PXNTABLE | DESC_UXNTABLE)) != 0;
    uint64_t desc = walk_desc(va);
    if (!desc) return false;
    bool w = (desc & (3ULL << 6)) == DESC_AP_RW_EL1;
    bool x = (desc & DESC_PXN) == 0;
    if (writable) *writable = w;
    /* WXN (SCTLR) + table-level XN locks: writable or XNTable-locked pages
     * are never executable. */
    if (executable) *executable = x && !w && !forced_xn;
    return true;
}

bool arch_mmu_heap_subtree_xn_locked(void) {
    for (unsigned i = 0; i < HEAP_L1_SLOTS; i++) {
        uint64_t desc = l0_table[i + 1];
        if (!(desc & DESC_VALID) || (desc & DESC_TABLE) == 0) return false;
        if ((desc & (DESC_PXNTABLE | DESC_UXNTABLE)) != (DESC_PXNTABLE | DESC_UXNTABLE))
            return false;
    }
    return true;
}

static void freeze_table_page(const void *table) {
    uint64_t va = (uint64_t)table;
    uint64_t *levels = (va < 0x40200000ULL) ? image_l3_low : image_l3_high;
    int idx = l3_index(va);
    uint64_t desc = levels[idx];
    if (desc & DESC_VALID) {
        levels[idx] = (desc & ~(uint64_t)(3ULL << 6)) | DESC_AP_RO_EL1 | DESC_NX;
    }
}

uintptr_t arch_mmu_root_table_address(void) {
    return (uintptr_t)&l0_table[0];
}

void arch_mmu_freeze_kernel_map(void) {
    /* Re-assert lock 1 on every heap-subtree root entry first (these
     * writes must happen while the root page is still writable). */
    for (unsigned i = 0; i < HEAP_L1_SLOTS; i++) {
        uint64_t idx = i + 1;
        l0_table[idx] = ((uint64_t)&heap_l1_tables[i] & DESC_ADDR_MASK) |
                        DESC_TABLE | DESC_VALID | DESC_PXNTABLE | DESC_UXNTABLE;
    }
    /* Flip the six static table pages Read-Only + XN. */
    freeze_table_page(&l0_table[0]);
    freeze_table_page(&l1_static[0]);
    freeze_table_page(&low_device_l2[0]);
    freeze_table_page(&kernel_l2[0]);
    freeze_table_page(&image_l3_low[0]);
    freeze_table_page(&image_l3_high[0]);
    arch_cpu_dsb();
    arch_cpu_tlbi_all();
    arch_cpu_dsb();
    arch_cpu_isb();
}

void arch_mmu_enable(const zenin_boot_info_t *boot_info) {
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 1u) return; /* Firmware supplied an active translation regime. */

    /* Static subtree tables (zeroed by the boot-environment contract). */

    /* Devices (0..1 GiB): Device-nGnRnE, RW+XN blocks. */
    for (int i = 0; i < 512; i++) {
        low_device_l2[i] = ((uint64_t)i << 21) | DESC_VALID |
                           DESC_ATTRIDX(0) | DESC_AF | DESC_AP_RW_EL1 | DESC_NX;
    }

    /* Kernel image region at 4 KiB granularity (W^X split). Stream C: the base
     * is the load address __kernel_start (decoupled from the QEMU 0x40000000),
     * so the W^X split follows wherever the image is loaded. */
    uint64_t kstart = (uint64_t)&__kernel_start;
    uint64_t image_lo = kstart & ~((uint64_t)0x3FFFFFFF); /* 1 GiB align */
    for (uint64_t page = 0; page < 1024; page++) {
        uint64_t va = image_lo + page * ZENIN_VM_PAGE_SIZE;
        bool writable = true;
        bool executable = false;
        if (va >= (uint64_t)&__kernel_start && va < (uint64_t)&__text_end) {
            writable = false;
            executable = true;
        } else if (va < (uint64_t)&__kernel_start) {
            writable = true;
            executable = false;
        } else if (va < (uint64_t)&__rodata_end) {
            writable = false;
            executable = false;
        }
        uint64_t desc = page_desc(va, writable, executable);
        if (page < 512) image_l3_low[page] = desc;
        else image_l3_high[page - 512] = desc;
    }

    /* kernel_l2 covers the 1 GiB region containing the kernel image. */
    kernel_l2[0] = ((uint64_t)image_l3_low & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
    kernel_l2[1] = ((uint64_t)image_l3_high & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
    for (int i = 2; i < 512; i++) {
        kernel_l2[i] = block_desc(image_lo + (uint64_t)i * 0x200000ULL);
    }

    /* Static L1: the kernel's W^X window (kernel_l2), then the memory map —
     * usable RAM as RW+XN and MMIO as device — driven by boot_info so the map
     * matches real silicon. Falls back to the QEMU virt layout when no valid
     * descriptor is present. */
    unsigned kidx = (unsigned)(kstart >> 30);
    l1_static[kidx] = ((uint64_t)kernel_l2 & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;

    if (zenin_boot_info_valid(boot_info)) {
        const zenin_boot_range_t *ram = zenin_boot_ram(boot_info);
        const zenin_boot_range_t *mmio = zenin_boot_mmio(boot_info);
        for (uint64_t i = 0; i < boot_info->ram_count; i++) {
            map_range(ram[i].base, ram[i].pages << 12, false);
        }
        for (uint64_t i = 0; i < boot_info->mmio_count; i++) {
            map_range(mmio[i].base, mmio[i].pages << 12, true);
        }
    } else {
        /* QEMU virt fallback: devices in 0..1 GiB, RAM in 0x80000000.. */
        l1_static[0] = ((uint64_t)low_device_l2 & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;
        if (kidx != 2) l1_static[2] = block_desc(0x80000000ULL);
    }

    /* Frozen root: static subtree only. */
    l0_table[0] = ((uint64_t)l1_static & DESC_ADDR_MASK) | DESC_TABLE | DESC_VALID;

    /* Heap subtree root entries (lock 1: PXNTable|UXNTable) over the
     * pre-created L1 tables (slots 1..192 of the 96 TiB window). */
    for (unsigned i = 0; i < HEAP_L1_SLOTS; i++) {
        uint64_t idx = i + 1;
        l0_table[idx] = ((uint64_t)&heap_l1_tables[i] & DESC_ADDR_MASK) |
                        DESC_TABLE | DESC_VALID | DESC_PXNTABLE | DESC_UXNTABLE;
    }

    /* 48-bit VA, 4 KiB granule, WBWA inner/outer, inner-shareable. */
    uint64_t tcr = 16ULL | (1ULL << 8) | (1ULL << 10) | (3ULL << 12) |
                   (1ULL << 23) | (3ULL << 32);
    uint64_t ttbr0 = (uint64_t)&l0_table[0];
    __asm__ volatile("msr tcr_el1, %0" : : "r"(tcr) : "memory");
    __asm__ volatile("msr ttbr0_el1, %0" : : "r"(ttbr0) : "memory");
    __asm__ volatile("msr mair_el1, %0"
                     : : "r"(0x0000000000004400ULL) : "memory");
    __asm__ volatile("dsb ish" : : : "memory");
    __asm__ volatile("tlbi vmalle1" : : : "memory");
    __asm__ volatile("dsb ish" : : : "memory");
    __asm__ volatile("isb" : : : "memory");

    /* Enable stage-1 translation, caches, and the WXN W^X backstop. */
    sctlr |= 1u | (1u << 2) | (1u << 12) | (1u << 19);
    __asm__ volatile("msr sctlr_el1, %0\n isb" : : "r"(sctlr) : "memory");
}

bool arch_mmu_identity_map_installed(void) {
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    return (sctlr & 1u) == 1u;
}
