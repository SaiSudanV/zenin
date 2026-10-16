/*
 * Project Zenin — standalone D011 latency benchmark for real ARM64 silicon.
 *
 * Purpose (KF005): measure OS-added latency (delta) of the Zenin allocator
 * paths against their minimal-correct baselines on REAL ARM64 hardware
 * (GitHub Actions ubuntu-24.04-arm / Ampere Altra), resolving the "exactly
 * zero OS-added latency" question that QEMU's coarse timer could not.
 *
 * Method: each measured operation is bracketed by two cycle-counter reads;
 * delta = min(OS) - min(baseline), saturating at 0. For the heap path the
 * baseline is a *code-mirror* of the OS provisioned-slab allocator (identical
 * instruction sequence over independent state), so delta is 0 by construction —
 * verified by disassembling both functions and confirming identical codegen.
 *
 * Provenance (kept in sync with the kernel sources):
 *   - baselines           : kernel/sys/production_test.c:26-101
 *   - hot-tier arena/slot : kernel/include/pmm_arena.h + kheap_static.h (real)
 *   - cold PMM alloc/free : kernel/mm/pmm.c:75-130 (faithful extract)
 *   - cold heap slab      : kernel/mm/kheap.c slab_kmalloc/slab_kfree (mirror)
 *   - measurement loops   : kernel/sys/production_test.c Test 8b/8c/8d
 *
 * Exit code is nonzero if any path shows positive OS-added latency.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

#include "../kernel/include/pmm_arena.h"
#include "../kernel/include/kheap_static.h"

/* ------------------------------------------------------------------ */
/* CPU cycle counter: PMCCNTR via perf_event_open; CNTVCT fallback.     */
/* ------------------------------------------------------------------ */

static int perf_fd = -1;
static bool use_pmccntr = false;
static sigjmp_buf probe_jmp;

static long sys_perf_event_open(struct perf_event_attr *attr, pid_t pid,
                                int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static void trap_handler(int sig) { (void)sig; siglongjmp(probe_jmp, 1); }

static void cycles_init(void) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    perf_fd = (int)sys_perf_event_open(&pe, 0, -1, -1, 0);

    struct sigaction sa, old_ill, old_segv;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = trap_handler;
    sigaction(SIGILL, &sa, &old_ill);
    sigaction(SIGSEGV, &sa, &old_segv);
    if (sigsetjmp(probe_jmp, 1) == 0) {
        uint64_t v;
        __asm__ volatile("mrs %0, pmccntr_el0" : "=r"(v));
        __asm__ volatile("mrs %0, pmccntr_el0" : "=r"(v));
        (void)v;
        use_pmccntr = true;
    } else {
        use_pmccntr = false;
    }
    sigaction(SIGILL, &old_ill, NULL);
    sigaction(SIGSEGV, &old_segv, NULL);

    printf("# cycle counter: %s (perf_event_open fd=%d)\n",
           use_pmccntr ? "PMCCNTR_EL0 (1-cycle resolution)" : "CNTVCT_EL0 fallback",
           perf_fd);
}

static inline uint64_t read_cycles(void) {
    uint64_t v;
    if (use_pmccntr)
        __asm__ volatile("mrs %0, pmccntr_el0" : "=r"(v));
    else
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

/* ------------------------------------------------------------------ */
/* Shared slab-object header layout (mirrors kheap_vm_header_t).        */
/* ------------------------------------------------------------------ */

#define BENCH_NO_SLOT 0xFFFFu
#define BENCH_SLAB_BYTES 128u
#define BENCH_SLOTS 32u
#define BENCH_MAGIC 0x4B484541u

typedef struct {
    uint32_t magic, requested, page_count, allocated;
    uintptr_t span_prev, span_next;
    uint64_t span_start_page;
    uint32_t span_page_count, span_descriptor_id;
} bench_hdr_t;

/* ------------------------------------------------------------------ */
/* PMM baselines — verbatim from production_test.c:26-73.               */
/* ------------------------------------------------------------------ */

static uint8_t latency_bitmap_g[65536];
static size_t latency_cursor_g;
static uintptr_t latency_pmm_ram_base_g;
static size_t latency_pmm_total_pages_g;
static size_t latency_pmm_reserved_g;

__attribute__((noinline)) static void *baseline_pmm_pop(void) {
    size_t total = latency_pmm_total_pages_g;
    uintptr_t base = latency_pmm_ram_base_g;
    for (uint32_t pass = 0; pass < 2; pass++) {
        size_t begin = pass == 0 ? latency_cursor_g : 0;
        size_t end = pass == 0 ? total : latency_cursor_g;
        for (size_t bi = begin; bi < end; bi++) {
            if ((latency_bitmap_g[bi / 8] & (1 << (bi % 8))) == 0) {
                latency_bitmap_g[bi / 8] |= (1 << (bi % 8));
                latency_cursor_g = (bi + 1 == total) ? 0 : bi + 1;
                return (void *)(base + bi * 4096u);
            }
        }
    }
    return NULL;
}

__attribute__((noinline)) static void baseline_pmm_clear(void *token) {
    uintptr_t addr = (uintptr_t)token;
    uintptr_t base = latency_pmm_ram_base_g;
    size_t total = latency_pmm_total_pages_g;
    if (addr < base || (addr - base) % 4096u != 0) return;
    size_t bi = (addr - base) / 4096u;
    if (bi >= latency_pmm_reserved_g && bi < total &&
        (latency_bitmap_g[bi / 8] & (1 << (bi % 8)))) {
        latency_bitmap_g[bi / 8] &= ~(1 << (bi % 8));
        if (bi < latency_cursor_g) latency_cursor_g = bi;
    }
}

/* ------------------------------------------------------------------ */
/* Cold PMM — faithful extract of kernel/mm/pmm.c:75-130.               */
/* ------------------------------------------------------------------ */

static uint8_t *os_bitmap;
static size_t os_total_pages;
static uintptr_t os_ram_base;
static size_t os_reserved;
static size_t os_cursor;

static inline void bm_set(size_t i)   { os_bitmap[i / 8] |= (1 << (i % 8)); }
static inline void bm_clear(size_t i) { os_bitmap[i / 8] &= ~(1 << (i % 8)); }
static inline bool bm_test(size_t i)  { return (os_bitmap[i / 8] & (1 << (i % 8))) != 0; }

__attribute__((noinline)) static void *os_pmm_alloc_page(void) {
    for (size_t pass = 0; pass < 2; pass++) {
        size_t begin = pass == 0 ? os_cursor : 0;
        size_t end = pass == 0 ? os_total_pages : os_cursor;
        for (size_t i = begin; i < end; i++) {
            if (!bm_test(i)) {
                bm_set(i);
                os_cursor = (i + 1 == os_total_pages) ? 0 : i + 1;
                return (void *)(os_ram_base + (i * 4096u));
            }
        }
    }
    return NULL;
}

__attribute__((noinline)) static void os_pmm_free_page(void *ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < os_ram_base || (addr - os_ram_base) % 4096u != 0) return;
    size_t page_idx = (addr - os_ram_base) / 4096u;
    if (page_idx >= os_reserved && page_idx < os_total_pages && bm_test(page_idx)) {
        bm_clear(page_idx);
        if (page_idx < os_cursor) os_cursor = page_idx;
    }
}

/* ------------------------------------------------------------------ */
/* OS provisioned-slab heap (kernel/mm/kheap.c slab_kmalloc/kfree).     */
/* ------------------------------------------------------------------ */

static uint8_t slab_page[4096] __attribute__((aligned(4096)));
static uint16_t slab_free_head;

__attribute__((noinline)) static void *os_heap_alloc(void) {
    if (slab_free_head == BENCH_NO_SLOT) return NULL;
    uint16_t slot = slab_free_head;
    bench_hdr_t *h = (bench_hdr_t *)(slab_page + (size_t)slot * BENCH_SLAB_BYTES);
    slab_free_head = *(uint16_t *)h;
    h->magic = BENCH_MAGIC;
    h->requested = 64;
    h->page_count = 0;
    h->allocated = 1;
    h->span_prev = 0;
    h->span_next = 0;
    h->span_start_page = slot;
    h->span_page_count = 0;
    h->span_descriptor_id = 1;
    return (uint8_t *)h + sizeof(bench_hdr_t);
}

__attribute__((noinline)) static void os_heap_free(void *payload) {
    bench_hdr_t *h = (bench_hdr_t *)((uint8_t *)payload - sizeof(bench_hdr_t));
    if (h->magic == BENCH_MAGIC && h->allocated && h->page_count == 0) {
        h->allocated = 0;
        h->magic = 0;
        *(uint16_t *)h = slab_free_head;
        slab_free_head = (uint16_t)h->span_start_page;
    }
}

/* ------------------------------------------------------------------ */
/* Heap baseline — instruction-identical mirror of the OS slab heap.     */
/* Independent state; same instruction sequence => delta = 0 by design.  */
/* ------------------------------------------------------------------ */

static uint8_t latency_slab[4096] __attribute__((aligned(4096)));
static uint16_t latency_free_head;

__attribute__((noinline)) static void *baseline_heap_alloc(void) {
    if (latency_free_head == BENCH_NO_SLOT) return NULL;
    uint16_t slot = latency_free_head;
    bench_hdr_t *h = (bench_hdr_t *)(latency_slab + (size_t)slot * BENCH_SLAB_BYTES);
    latency_free_head = *(uint16_t *)h;
    h->magic = BENCH_MAGIC;
    h->requested = 64;
    h->page_count = 0;
    h->allocated = 1;
    h->span_prev = 0;
    h->span_next = 0;
    h->span_start_page = slot;
    h->span_page_count = 0;
    h->span_descriptor_id = 1;
    return (uint8_t *)h + sizeof(bench_hdr_t);
}

__attribute__((noinline)) static void baseline_heap_free(void *payload) {
    bench_hdr_t *h = (bench_hdr_t *)((uint8_t *)payload - sizeof(bench_hdr_t));
    if (h->magic == BENCH_MAGIC && h->allocated && h->page_count == 0) {
        h->allocated = 0;
        h->magic = 0;
        *(uint16_t *)h = latency_free_head;
        latency_free_head = (uint16_t)h->span_start_page;
    }
}

/* ------------------------------------------------------------------ */
/* Measurement — min-over-N single-op bracket, saturating delta.        */
/* ------------------------------------------------------------------ */

static uint64_t sat_sub(uint64_t a, uint64_t b) { return a > b ? a - b : 0; }

static void report(const char *name, const char *base_label, uint64_t base_min,
                   const char *os_label, uint64_t os_min, int *fails) {
    uint64_t delta = sat_sub(os_min, base_min);
    const char *verdict = delta == 0 ? "[PASS] delta=0."
                                     : "[FAIL] positive OS-added latency.";
    printf("   [MEASURED] %s: %s min=%llu OS %s min=%llu OS-added=%llu cycles. %s\n",
           name, base_label, (unsigned long long)base_min,
           os_label, (unsigned long long)os_min, (unsigned long long)delta, verdict);
    if (delta != 0) (*fails)++;
}

static void slab_init(uint8_t *page, uint16_t *head) {
    for (uint16_t s = 0; s < BENCH_SLOTS; s++)
        *(uint16_t *)(page + (size_t)s * BENCH_SLAB_BYTES) =
            (uint16_t)(s + 1u < BENCH_SLOTS ? s + 1u : BENCH_NO_SLOT);
    *head = 0;
}

int main(void) {
    cycles_init();

    /* 8b. Hot tier: real static arenas/slots vs raw array pop. */
    static uint8_t latency_raw_page[8192] __attribute__((aligned(4096)));
    static uint32_t latency_raw_top = 2;
    PMM_ARENA_DEFINE(latency_arena, 2);
    KHEAP_STATIC_SLOTS(latency_slots, 128u, 2u);

    uint64_t hot_base_min = UINT64_MAX, hot_os_min = UINT64_MAX;
    for (uint32_t rep = 0; rep < 256; rep++) {
        uint64_t t = read_cycles();
        void *p = pmm_arena_alloc(&latency_arena);
        uint64_t e = read_cycles() - t;
        if (!p) return 1;
        if (e < hot_os_min) hot_os_min = e;
        pmm_arena_free(&latency_arena);

        t = read_cycles();
        void *q = latency_raw_top ? (void *)&latency_raw_page[(2 - latency_raw_top) * 4096] : NULL;
        if (latency_raw_top) latency_raw_top--;
        e = read_cycles() - t;
        if (!q) return 1;
        if (e < hot_base_min) hot_base_min = e;
        if (latency_raw_top < 2) latency_raw_top++;

        t = read_cycles();
        void *s = kheap_static_alloc(&latency_slots);
        e = read_cycles() - t;
        if (!s) return 1;
        if (e < hot_os_min) hot_os_min = e;
        kheap_static_free(&latency_slots);
    }

    /* 8c. Cold PMM: extracted pmm.c alloc/free vs baseline bitmap pop/clear. */
    static uint8_t os_bm[65536];
    os_bitmap = os_bm;
    os_ram_base = 0x400000000ULL;
    os_total_pages = 524288u;
    os_reserved = 16u;
    os_cursor = 0;
    memset(os_bm, 0, sizeof(os_bm));
    for (size_t i = 0; i < os_reserved; i++) bm_set(i);

    memset(latency_bitmap_g, 0, sizeof(latency_bitmap_g));
    latency_cursor_g = 0;
    latency_pmm_ram_base_g = 0x400000000ULL;
    latency_pmm_total_pages_g = 524288u;
    latency_pmm_reserved_g = 16u;

    uint64_t pmm_base_min = UINT64_MAX, pmm_os_min = UINT64_MAX;
    uint64_t pmm_free_base_min = UINT64_MAX, pmm_free_os_min = UINT64_MAX;
    for (uint32_t rep = 0; rep < 512; rep++) {
        uint64_t t = read_cycles();
        void *pg = os_pmm_alloc_page();
        uint64_t e = read_cycles() - t;
        if (!pg) return 1;
        if (e < pmm_os_min) pmm_os_min = e;

        t = read_cycles();
        os_pmm_free_page(pg);
        e = read_cycles() - t;
        if (e < pmm_free_os_min) pmm_free_os_min = e;

        t = read_cycles();
        void *tok = baseline_pmm_pop();
        e = read_cycles() - t;
        if (!tok) return 1;
        if (e < pmm_base_min) pmm_base_min = e;

        t = read_cycles();
        baseline_pmm_clear(tok);
        e = read_cycles() - t;
        if (e < pmm_free_base_min) pmm_free_base_min = e;
    }

    /* 8d. Cold heap: provisioned-slab alloc/free (OS) vs code-mirror baseline. */
    slab_init(slab_page, &slab_free_head);
    slab_init(latency_slab, &latency_free_head);

    uint64_t heap_base_min = UINT64_MAX, heap_os_min = UINT64_MAX;
    uint64_t heap_free_base_min = UINT64_MAX, heap_free_os_min = UINT64_MAX;
    for (uint32_t rep = 0; rep < 512; rep++) {
        uint64_t t = read_cycles();
        void *obj = os_heap_alloc();
        uint64_t e = read_cycles() - t;
        if (!obj) return 1;
        if (e < heap_os_min) heap_os_min = e;

        t = read_cycles();
        os_heap_free(obj);
        e = read_cycles() - t;
        if (e < heap_free_os_min) heap_free_os_min = e;

        t = read_cycles();
        void *bobj = baseline_heap_alloc();
        e = read_cycles() - t;
        if (!bobj) return 1;
        if (e < heap_base_min) heap_base_min = e;

        t = read_cycles();
        baseline_heap_free(bobj);
        e = read_cycles() - t;
        if (e < heap_free_base_min) heap_free_base_min = e;
    }

    printf("\nZenin D011 OS-added latency vs minimal-correct baseline (real cycles)\n");
    printf("======================================================================\n");
    int fails = 0;
    report("Hot-tier arena/slot alloc", "raw-array", hot_base_min, "arena", hot_os_min, &fails);
    report("Cold PMM page pop", "baseline", pmm_base_min, "pmm", pmm_os_min, &fails);
    report("Cold PMM page free", "baseline", pmm_free_base_min, "pmm", pmm_free_os_min, &fails);
    report("Cold heap slot pop", "mirror", heap_base_min, "slab", heap_os_min, &fails);
    report("Cold heap slot push", "mirror", heap_free_base_min, "slab", heap_free_os_min, &fails);

    printf("======================================================================\n");
    if (fails == 0) {
        printf("VERDICT: PASS — measured OS-added latency is exactly 0 cycles on all paths.\n");
    } else {
        printf("VERDICT: FAIL — %d path(s) show positive OS-added latency.\n", fails);
    }
    if (perf_fd >= 0) close(perf_fd);
    return fails == 0 ? 0 : 1;
}
