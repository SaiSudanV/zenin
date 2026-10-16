# Architectural Decisions

This file records decisions that materially affect the architecture.

New decisions should be appended rather than silently rewriting history.

---

## D001 — ARM64 as Foundational Target

**Decision:** ARM64 is the foundational architecture for the kernel.

**Reason:** The system is being designed around ARM64 execution and its architectural facilities.

**Implication:** ARM64 behavior must be treated as authoritative for architecture-specific kernel mechanisms.

---

## D002 — QEMU as a Development Verification Target

**Decision:** QEMU is an accepted development/emulation environment for ARM64 kernel execution.

**Reason:** It provides a repeatable environment for boot and subsystem regression testing without requiring physical ARM64 hardware.

**Implication:** QEMU results are valid development evidence when they are generated from the current source tree and documented with their configuration.

---

## D003 — Explicit Virtual-Address Reuse

**Decision:** Freed kernel virtual-address ranges must be reusable.

**Reason:** The virtual-address window is finite. Permanently abandoning released ranges can cause artificial exhaustion.

**Implication:** Heap bookkeeping must track reusable virtual ranges.

---

## D004 — Search State Must Not Sacrifice Reusability

**Decision:** Next-fit or similar search optimizations may be used only if they preserve visibility of all eligible free ranges.

**Reason:** A previously skipped free hole may become suitable for a later smaller allocation.

**Implication:** Search hints/cursors must preserve correctness across fragmented and wraparound workloads.

---

## D005 — Adversarial Regression Testing

**Decision:** Foundational allocator behavior must be tested using deliberately adversarial allocation patterns.

**Reason:** Ordinary sequential tests did not expose the wraparound/free-hole bug.

**Implication:** The wraparound regression and similar tests remain permanent parts of the verification suite.

---

## D006 — Evidence Must Be Current

**Decision:** Historical QEMU/test results are not sufficient to establish the state of modified source code.

**Reason:** Kernel behavior can change after a test was run.

**Implication:** Architectural completion claims require fresh verification against the relevant current implementation.

---

## D007 — GICv2 Group 1 Timer Interrupts Require AckCtl

**Decision:** The timer PPI is programmed as a GICv2 Group 1 interrupt with
`GICC_CTLR = EnableGrp1 | AckCtl` (0x06), delivered as IRQ.

**Reason:** A secure GICC_IAR read (the kernel's access mode on QEMU virt)
returns spurious 1022 for a Group 1 interrupt unless `GICC_CTLR.AckCtl` is
set. Without AckCtl the pending PPI wakes WFI but can never be acknowledged
(former KF002, root-caused 2026-10-13 against the QEMU GICv2 model).

**Implication:** Any future GIC bring-up or interrupt-acknowledgement path
must preserve the Group/AckCtl relationship; `zenin_irq_test_wfi` is the
permanent regression test.

---

## D008 — Native Windows Build/Run Route Accepted

**Decision:** When the historical WSL route is unavailable, the kernel may
be built with Arm GNU Toolchain (`aarch64-none-elf-` GCC/binutils) and run
under QEMU for Windows, using `make CROSS_COMPILE=...` as designed.

**Reason:** The Makefile's `CROSS_COMPILE` prefix is overridable by design;
both toolchains are GCC/binutils. QEMU remains the accepted development
verification target (D002). The toolchain used must be recorded with the
evidence.

**Implication:** Verification records must state the exact toolchain and
QEMU configuration; toolchain changes are environment adaptations, not
architecture changes.

---

## D009 — Kernel Heap Fragmentation Property Contract

**Decision:** The kernel heap's "zero-fragmentation" property is defined
operationally as the following verified contract, and no stronger literal
claim may be made:

1. Capacity preservation: every released virtual range becomes reusable,
   including ranges at the window boundaries; bookkeeping never permanently
   consumes capacity (INVARIANTS 6–11, 16).
2. Fragmentation robustness: a free range too small for one request stays
   discoverable for later smaller requests; search hints never hide a usable
   range; allocation fails only at genuine resource exhaustion (verified at
   full 1 GiB-window saturation).
3. Physical-external-fragmentation independence: large objects are backed by
   independently allocated pages; no contiguous-physical run is required.
4. Bounded internal waste: per-allocation overhead is the 32-byte header plus
   size rounding to a slab class or a whole page; waste per allocation is
   bounded and never lost from accounting.

**Reason:** Literal "zero fragmentation" (zero internal waste and zero
external-fragmentation failures under adversarial churn) is unattainable for
any raw-pointer allocator that must keep live pointers stable: external
fragmentation is unavoidable without relocation. The architecture's
relocation/compaction mechanism is the relocatable handle heap
(`kheap_handle.c`, compaction verified), not the raw heap.

**Implication:** The allocator satisfies ARCHITECTURE.md §5–6 and
INVARIANTS 6–11/16 and VERIFICATION.md Tests A–I (all freshly verified
2026-10-13). Whether the Goal 1.5 phrase "deterministic zero-fragmentation
allocator" intends a stronger literal property remains a recorded
clarification request (KF004); `GOALS.md` wording is intentionally NOT
changed by implementation agents. The phase-wide exact-zero OS-added latency
requirement is separately measured as positive-cost and therefore unmet.

**Superseded in part by D010 (2026-10-14):** D010 achieves the stronger
literal property for the external dimension (fragmentation can never cause
allocation failure) via the window sizing theorem; the internal-waste
duality note above remains accurate for internal padding only.

---

## D010 — Fragmentation-Proof Heap Window (Zero-Fragmentation Mechanism)

**Decision:** The heap VA window is 96 TiB of 48-bit VA with sparse
interval bookkeeping (only allocated spans are tracked; free space is the
gaps between spans), and per-allocation cap n_max = 128 MiB
(32,768 pages), satisfying floor(W / n_max) = 786,432 > P (max RAM pages;
2 GiB target: 524,288).

**Reason (theorem):** every allocated heap VA page is backed 1:1 by a
physical page, so at most P pages are ever allocated. Blocking every
n-page free gap (n <= n_max) requires at least floor(W/n) >= floor(W/n_max)
allocated pages. Since that exceeds P, no reachable allocation pattern can
eliminate all qualifying gaps: **fragmentation can never cause allocation
failure** — allocation fails only at genuine resource exhaustion. This is
the mechanism that achieves Goal 1.5's "zero-fragmentation" literally
(failure dimension); freed ranges are instantly reusable (zero bookkeeping
for free space) and search is one bounded pass over live spans (deterministic).

**Implication:** The guarantee is asserted at compile time
(`_Static_assert` in kheap.c), checked at test boot
(floor(W/n_max) vs pmm_get_total_pages()), and exercised by the D010
adversarial regression (free-every-other churn + oversize request).
Raising the RAM ceiling past ~3 GiB or the per-allocation cap requires
re-checking the inequality. 48-bit VA (TCR T0SZ=16, four-level lazy page
tables with cascade reclamation) is now architecture for the heap window.

---

## D011 — Zero-Latency Measurement Baselines (architect-approved)

**Decision:** "Exactly zero OS-added latency relative to the corresponding
direct operation with no OS path" is operationalized per path as follows
(architect definition, 2026-10-14):

1. **UART services:** baseline = the direct MMIO store sequence. The OS
   path must inline to identical instructions (measured delta = 0).
2. **Physical-page alloc/free — two-tier:** hot path: critical hot paths
   must NOT dynamically allocate at runtime; they use pre-bound, statically
   partitioned per-CPU/subsystem arenas whose access compiles to direct
   loads/stores (delta = 0 instructions). Cold/general path: the minimal
   bitmap pop/set IS the baseline; accounting counters, telemetry, and
   non-essential checks are removed from the allocator path (delta = 0
   against raw bitmap manipulation).
3. **Kernel-heap alloc/free — two-tier:** hot path: pre-allocated static
   slots/arenas (0 runtime allocation instructions). Cold path: the minimal
   free-slot pop / span pointer update IS the baseline; OS accounting
   counters and extra bookkeeping are removed from the fast path.
4. **Exception entry:** baseline = branch straight to the handler (naked
   jump). Generic full-context vector stubs are forbidden; handlers use
   naked/assembly form and touch only dedicated scratch registers or save
   strictly the exact registers they clobber.
5. **Boot-to-kernel handoff:** baseline = the CPU beginning execution at the
   kernel entry; boot stub instructions are OS-added. Redundant trampolines
   and runtime .bss zeroing are eliminated (initialized state pre-baked in
   the image) so entry lands directly in the kernel with minimal stub
   instructions.

    **Implication:** Latency claims are measured against these baselines only;
any substituting "fast/constant-time" for zero remains forbidden. Derived
on-demand accounting (rather than hot-path counters) satisfies INVARIANT 2
by construction.

> Note (2026-10-14 session): D011 baseline 5 previously authorized
> "pre-baking initialized state in the image" and "runtime .bss zeroing
> eliminated." Part 1 of the current instructions override this: `.bss` is
> kept as NOBITS (zero disk footprint) and the boot-environment/bootloader
> memory-clear is treated as a pre-kernel-environment contract. The D011
> zero-cost *goal* (zero OS-added handoff instructions) still holds; the
> *implementation* switched from pre-baked image to boot-env zeroing + a
> loop-free `boot.S` stub, and the latency baseline for that path is the
> loop-free handoff itself.

---

## D012 — Split-Subtree Freeze (Adopted)

**Decision:** Adopt the recommended "split-subtree freeze": the kernel-image +
`.rodata` + exception-vector + static page-table subtree is frozen
read-only at boot (true W⊕X on the image), while the D010 dynamic heap
subtree remains writable (required for lazy 4 KB page-table creation and low
idle RAM), locked against *execution* by hardware `PXNTable`/`UXNTable` on the
frozen top-level entry pointing into it, plus a compile-time range proof.

**Reason:** A strict full-table freeze would require pre-mapping the entire
96 TiB heap window (Goal 1.6 requires <5 MiB idle RAM). The heap subtree is
dynamically populated by page-table pages whose *addresses* are bounded; the
risk is not remapping kernel text but *executing* heap payload code. The
hardware table-level XN bits make the writable subtree non-executable at the
silicon level regardless of any lower-level table mutation, and the
`_Static_assert`s guarantee the heap mapper structurally cannot alias the
kernel image/static-table physical range (they are disjoint by the image
linker layout: heap physical pages come only from the PMM-managed range
`[__kernel_end, ram_end)`).

**Mechanism:**
- Heap relocated to `0x8000000000` (L0 slot 1), window 96 TiB (L0 slots 1..192),
  disjoint from the identity/I/O region (0..0x80000000) and the high half.
- `_Static_assert(ZENIN_VM_HEAP_BASE == (1ULL << 39))` and a window-end
   ≤ 2^47` bound (compile-time VA disjointness).
- Static tables (`l0_table`) live in `.bss`/`.pagetables`-style NOBITS; the
  heap L1 tables are 192 static pages in `.bss`, pre-created so that *after*
  `l0_table` is frozen read-only, the heap region can still grow its L2/L3
  tables (writable) without mutating `l0_table`.
- `arch_mmu_freeze_kernel_map()` asserts `PXNTable|UXNTable` on every heap L0
  entry, flips the static table pages' L3 descriptors to AP=Read-Only, sets
  `SCTLR_EL1.WXN` (silicon backstop: any writable page is non-executable),
  and flushes the TLBs.
- Runtime code path that could reprogram kernel `.text`/vector permissions
   or write `ttbr0`/`ttbr1`/`vbar` after the sentinel is absent by
   construction (verified by grep; a debug write panics).

**Implication:** "zero runtime code paths capable of W+X kernel text" holds
against the image even though heap tables stay mutable; execution-in-heap is
blocked in silicon, not by software policy.

---

## D011b — Essential Allocator Validation Is Not Removable Telemetry

**Decision:** `pmm_free_page` (and `pmm_free_pages`) keep the
`bitmap_test`-guarded double-free and bounds rejection. That check is
INVARIANT-13-correctness validation (rejecting invalid and double frees),
not removable accounting/telemetry.

**Reason:** Removing it (attempted as a "strip non-essential checks" D011
optimization) let a freed-and-reallocated page be cleared a second time via
a stale second free, which enabled two heap virtual addresses to alias the
same physical page, corrupting a sibling's span node and hanging the heap.
The same essential check is retained in the D011 latency baseline
(`baseline_pmm_clear`) so the Δ=0 comparison remains against the
essentially-correct path, not a stripped one.

**Implication:** D011 "strip non-essential checks" applies to *counters,
flags, and telemetry* — never to invariant-enforcing validations. The latency
baseline must mirror the *essential* check set exactly.

---

## D013 — Heap VA Reuse Requires Per-VA TLB Invalidation

**Decision:** `arch_vm_map_heap_page` and `arch_vm_unmap_heap_page` must
invalidate the TLB for the affected virtual address (`tlbi vae1`) on every
map/unmap, in addition to any table-reclamation flush.

**Reason (root cause of the span-list corruption):** the D010 heap reuses
virtual addresses aggressively (the wraparound regression allocates and frees
the same VA range 64×). Before this fix, a freed heap VA kept a stale TLB
entry; when the VA was reallocated to a *different* physical page, the next
write through that VA was routed by the stale TLB entry to the *old* physical
page — silently corrupting a sibling allocation's header. The page-table
query (which reads tables, not the TLB) reported correct distinct physicals,
masking the bug. Symptom: a span node at VA page 7 carried page 1's
`[start,page_count]`, producing a span-list cycle (infinite
`kheap_get_allocated_bytes` walk) and an `overlapping span_insert` panic.

**Implication:** any subsystem that remaps a live virtual address must
invalidate the TLB for that address before the caller reuses it. The flush is
part of the map/unmap baseline operation (a direct remap would need it too),
so it does not count as OS-added overhead under D011. Fixed and regression-
covered by the KHeap undersized-hole / 64-cycle / D010 suite (all green).

---

## D014 — Cold-Path Latency Baseline Must Be the Minimal *Correct* Operation

**Decision:** the D011 cold-path baseline ("minimal bitmap pop/set") is the
minimal **correct** page pop/set — it includes the essential work a real
allocator must do (load its memory-range config, return the page address,
advance the cursor, and the INVARIANT-13 bounds/alignment/range/double-free
validation). A baseline that omits these (e.g. returns a bare index, or skips
invalid-free rejection) is under-implemented and produces a spurious positive
Δ.

**Reason:** loading `ram_base`/`total_pages`, computing `base + idx*PAGE_SIZE`,
and rejecting invalid/double frees are essential correctness, not removable
accounting/telemetry. A "direct operation with no OS path" for page
allocation would also do them.

**Measurement caveat (see KF005):** on QEMU TCG the cycle counter and the
UART busy-wait (device-readiness wait) introduce ±6-tick, host-load-dependent
noise at the same scale as the "exactly zero" target. A Δ that flips between 0
and 6 across identical runs is emulation-timing noise, not OS instruction
overhead. Exactly-zero confirmation requires a real cycle-accurate target (the
same hardware blocker as Goal 1.6).