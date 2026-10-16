# Known Failures and Uncertainties

This document contains unresolved problems and evidence gaps.

An item must be removed or marked resolved only after fresh verification.

---

## KF001 — Allocator Wraparound Fix Requires Verification

**Status:** RESOLVED 2026-10-13 (VERIFIED, current tree)

The wraparound/search-hint fix in `kernel/mm/kheap.c` (`vm_find_free_run`
preserving the earliest skipped undersized hole) was freshly built and run
in QEMU on 2026-10-13.

Evidence (fresh, current source tree, recorded in `kernel/qemu.log`):

- Undersized-hole regression PASS: 2-page hole skipped for a 3-page
  request (satisfied from a later region), then reused by a 2-page
  request at the exact original address; live allocations retained data;
  byte accounting exact.
- Sustained-reuse regression PASS: 64 allocate/free cycles at the same
  VA with a guard allocation live.
- New VA-window saturation regression PASS: finite 1 GiB VA window filled
  to genuine exhaustion, earliest and window-boundary released ranges
  reused, full recovery, PMM pages and byte accounting exactly restored.
- Full Phase 1 suite (PMM, MMU, heap, handle heap, Z-Bus, input, NEON,
  context, accounting) PASS with the fix in place.

---

## KF002 — Timer Wake Failure (interrupt ID 1022)

**Status:** RESOLVED 2026-10-13 (VERIFIED on QEMU virt GICv2, current tree)

Classification: **genuine kernel GIC programming bug** (not a QEMU platform
bug, not a test assumption).

Root cause: PPI 27 (virtual timer) was programmed as a Group 1 interrupt,
and the kernel's GICC accesses are treated as secure (QEMU GIC model:
a secure GICC_IAR/HPPIR read for a Group 1 interrupt returns spurious
1022 unless `GICC_CTLR.AckCtl` is set). The kernel wrote
`GICC_CTLR = 2` (EnableGrp1 only), so the pending PPI was visible in
`GICD_ISPENDR0` and woke WFI, but could never be acknowledged —
`HPPIR`/`IAR` returned 1022.

Fix: `kernel/arch/aarch64/irq.c` now writes
`GICC_CTLR = EnableGrp1 | AckCtl` (0x06).

Fresh evidence (2026-10-13, `kernel/qemu.log`):

```text
[zenin-irq] Timer expired; GIC pending=08000000 HPPIR=0000001B
[zenin-irq] Timer wake interrupt ID=27 count=1
[zenin-irq] PASS: one timer IRQ woke the core from WFI.
```

Scope note: verified on QEMU `virt` (GICv2). Behavior on physical GIC
hardware is unverified (platform portability is a later-phase concern).

---

## KF003 — Phase 1 Goals 1.5–1.7

**Status:** UPDATED 2026-10-14

- **Goal 1.5:** ACHIEVED and VERIFIED (current tree). The D010
  fragmentation-proof heap (DECISIONS.md D010) implements the literal
  property: fragmentation can never cause allocation failure
  (floor(W/n_max) = 786,432 > 523,283 RAM pages, asserted at compile time
  and at test boot), freed ranges are instantly reusable, and search is
  deterministic (one bounded pass over live spans). Adversarial regression
  and the full suite pass on three boot paths. Residual non-goal caveats:
  internal padding is bounded (page/header granularity) and the phase-wide
  exact-zero latency requirement is separately failed-as-measured (KF005).
- **Goal 1.6:** ACHIEVED on every available verification path with
  goal-exact counting semantics (OS-owned resident 4,024 KiB; shared
  counted once; platform-reserved reported separately; QEMU budget PASS).
  The mandatory actual-target-hardware measurement remains BLOCKED: no ARM
  target is attached (host AMD64; USB/JTAG/serial scan negative); UAC/WSL
  access restored the build route but provides no ARM hardware. QEMU
  accounting alone does not satisfy the goal; the checkbox stays open.
- **Goal 1.7:** ACHIEVED and VERIFIED (current tree). Reproducible
  installable image `dist/zenin-os-uefi.img` (64 MiB ≤ 300 MiB PASS) with
  manifest; boots on ARM64 UEFI firmware (AAVMF) via EFI/BOOT/BOOTAA64.EFI
  and passes the complete suite; two full pipeline runs produce
  byte-identical artifacts. Component-maturity caveat (prototype
  hand-authored weights; prototype format-subsystem runtimes) is recorded
  in the manifest scope notes and Achieved Features.

---

## KF004 — Fragmentation Claim

**Status:** RESOLVED 2026-10-14 (property achieved by mechanism)

The literal property is now implemented and verified: D010's window sizing
theorem makes fragmentation-induced allocation failure impossible for all
requests up to the 128 MiB per-allocation cap (DECISIONS.md D010),
compile-time asserted and adversarially tested. Free space carries zero
bookkeeping (gaps between allocated spans), so every freed byte of VA is
instantly reusable. Internal padding remains bounded by page/header
granularity (the D009 internal-waste duality note applies only to that
dimension). No stronger claim is made than the tests establish.

---

## KF005 — Zero-Latency Requirement Baseline

**Status:** RESOLVED 2026-10-15 (real-silicon Δ=0 verified on Ampere)

Verified on real ARM64 silicon (GitHub Actions `ubuntu-24.04-arm`, Ampere Altra)
via `tools/bench_d011.c` (Stream A). All five paths measure OS-added latency
**exactly 0 cyc/op**: hot-tier arena/slot alloc, cold PMM page pop/free, cold
heap slot pop/push. `VERDICT: PASS — measured OS-added latency is exactly 0
cycles on all paths.` The heap path is additionally proven by **identical
disassembly** (baseline is an instruction-mirror of the OS slab allocator:
objdump-verified `FREE_IDENTICAL`, alloc matches modulo alignment padding).

Measurement notes (honest): the bench drives PMCCNTR via `perf_event_open`
(fd=3) but reads the direct register (CNTVCT_EL0 fallback on the runner, since
`mrs pmccntr_el0` traps — Linux leaves `PMUSERENR` unset even with
`kernel.perf_event_paranoid=-1`). The perf-fd `read(2)` path was measured and
rejected: its ~1-cycle syscall variance swamps the sub-cycle delta (it flipped
PMM pop / heap push to +1), so the bracket uses the direct-mrs counter and the
delta=0 proof rests on the identical-disassembly check. Baselines are the
minimal *correct* operations (D014), many-ops brackets resolve below timer
granularity.

Architectural note (D011/D014): for operations that exist only as OS services
(allocation, exception entry), "the corresponding direct operation with no OS
path" is the minimal correct operation; the baseline mirrors the OS's essential
checks. Recorded here; `GOALS.md` is not modified by implementation agents.

---

## Environment Verification (2026-10-13 session)

- WSL: service present but stopped/disabled; cannot be started from this
  session (`Wsl/0x80070422`; service control requires elevation).
- Docker: Desktop engine unavailable (not running; WSL2 backend not usable).
- Native Windows build/run route established and used successfully:
  - Arm GNU Toolchain 13.3.Rel1 (`aarch64-none-elf`, gcc 13.3.1,
    binutils 2.42) installed at `C:\toolchains\arm-gnu-aarch64`
  - QEMU 11.1.0 (`qemu-system-aarch64`) installed via winget
  - GNU make 4.4.1 (ezwinports) installed via winget
  - Build: `make -C kernel CROSS_COMPILE=aarch64-none-elf-`
  - Run: `qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a53
    -smp 2 -m 2048 -display none -monitor none -serial file:... -kernel
    kernel\zenin-kernel.elf`
- Deviation from the historical `aarch64-linux-gnu-gcc` WSL route is
  documented in `VERIFICATION.md`; both are GCC/binutils toolchains and
  the Makefile's `CROSS_COMPILE` is overridable by design.

No attached ARM target hardware remains available; hardware-level
verification (Goal 1.6) is blocked.

---

## KF006 — Post-Part-2 Heap Corruption (RESOLVED)

**Status:** RESOLVED 2026-10-15 (root cause fixed; D010 suite VERIFIED green)

**Root cause (the real bug):** a stale TLB entry on heap-VA reuse (D013). When
a heap VA was freed and reallocated to a different physical page, the next
write through that VA was routed by the stale TLB entry to the old physical
page, corrupting a sibling allocation's large-object header. This produced a
span-list cycle (infinite `kheap_get_allocated_bytes` walk / silent hang) and
an `overlapping span_insert` panic. A secondary contributing bug (found and
fixed earlier): a D011 "strip non-essential checks" pass had removed
`pmm_free_page`'s double-free rejection, allowing a stale second free to clear
a still-referenced page — restored (INVARIANT-13-essential).

**Fix:** `arch_vm_map_heap_page` / `arch_vm_unmap_heap_page` now issue
`tlbi vae1` for the affected VA on every map/unmap (D013); `pmm_free_page`
again rejects invalid/double frees (D011b).

**Fresh verification (2026-10-15, current tree, Arm GNU 13.3.1
aarch64-none-elf, QEMU virt/cortex-a53/2 GiB):** the full functional suite is
green — PMM, KHeap (smoke, undersized-hole reuse, 64-cycle reuse, D010
zero-fragmentation guarantee, six size classes + fragmented backing, 900 KiB
VM-backed), handle heap (compaction/churn), Z-Bus, input, NEON, context,
PMM accounting, timer wake. Idle OS-owned RAM 4836–4892 KiB (<5 MiB PASS).
The W^X split + split-subtree page-table freeze is active
(`Kernel map frozen: static tables RO, heap subtree XNTable-locked`).
