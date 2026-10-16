# Verification Strategy

## Purpose

This document defines how architectural claims are verified.

The goal is to prevent implementation claims from being accepted solely because source code appears plausible.

---

# Evidence Levels

## V0 — Unspecified

The behavior has not yet been defined sufficiently to test.

## V1 — Implemented

Relevant code exists.

This is not proof of correctness.

## V2 — Build Verified

The current source compiles successfully for the target.

This proves compilation only.

## V3 — Runtime Verified

The current source boots/runs and the relevant test executes successfully.

## V4 — Adversarially Verified

The behavior has survived deliberately constructed failure/boundary/fragmentation cases.

## V5 — Sustained/Regression Verified

The behavior remains correct under repeated workloads and the regression is permanently encoded in the test suite.

A foundational architectural claim should reach V3–V5 depending on its nature.

---

# Phase 1 Verification Matrix

| Area | Required Evidence |
|---|---|
| ARM64 kernel | Current-source ARM64 build + QEMU boot |
| PMM | Allocation/free + accounting checks |
| MMU | Mapping/translation/permission checks |
| Heap | Allocation/free correctness |
| VA reuse | Repeated release/reallocation |
| Fragmentation | Fragmented free-space allocation tests |
| Wraparound | Search crossing the end of the VA window |
| Undersized holes | Skip a hole, then later reuse it |
| Live allocations | Data-integrity checks during unrelated reuse |
| Sustained reuse | Repeated allocation/free cycles |
| Handle heap | Current runtime regression |
| Z-Bus | Current runtime regression |
| Input | Current runtime regression |
| NEON | Current runtime regression |
| Timer | Current runtime evidence + root-cause classification |

---

# Allocator Verification

The allocator must be tested in at least these scenarios.

## Test A — Basic allocation

Allocate valid sizes and verify returned regions are valid.

## Test B — Free and reuse

Allocate, free, then allocate again.

Verify that released resources can be reused.

## Test C — Live guard

Keep allocation A live while repeatedly allocating/freeing B.

Verify A's address and contents remain valid.

## Test D — Repeated large-object reuse

Repeatedly allocate and release a large object.

Verify that the allocator reuses the released VA range instead of monotonically advancing through the entire VA window.

## Test E — Fragmented holes

Construct:

```text
FREE → USED → FREE → USED → FREE
```

with different hole sizes.

Verify that an appropriate hole is selected.

## Test F — Undersized skipped hole

Construct:

```text
FREE(2)
USED
FREE(8)
```

Request 4 pages.

The first hole must be skipped because it is too small.

Then request 1 page.

The original 2-page hole must remain discoverable and reusable.

## Test G — Wraparound

Force the allocator's search cursor toward the end of the VA window and require it to wrap.

Verify that free ranges before the cursor remain discoverable.

## Test H — Data integrity

Keep multiple allocations live with recognizable data patterns.

Perform unrelated allocation/free operations.

Verify that all live data remains intact.

## Test I — Sustained churn

Repeat allocation/free cycles sufficiently many times to expose monotonically consumed VA space.

The allocator must fail only when the actual available resource is exhausted.

---

# Documentation Verification

Whenever an implementation changes:

1. Rebuild.
2. Rerun affected tests.
3. Rerun relevant integration tests.
4. Update evidence.
5. Reclassify claims.

Never leave a previously verified claim marked as current if the implementation that produced it has materially changed.

---

# QEMU Evidence Requirements

A QEMU result should record:

- source state,
- build command,
- architecture,
- machine type,
- CPU,
- SMP configuration,
- memory configuration,
- kernel image,
- test suite,
- PASS/FAIL output,
- relevant warnings/errors.

Historical logs must be labeled historical.

---

# Completion Rule

A Phase 1 goal is verified only when its acceptance criteria are mapped to concrete evidence.

If evidence is unavailable:

```text
IMPLEMENTED BUT UNVERIFIED
```

must be used instead of:

```text
PASS
```

---

# Verification Record — 2026-10-13 (current tree)

## Build

- Source: current workspace tree (`kernel/`), forced clean build.
- Command: `make -C kernel CROSS_COMPILE=aarch64-none-elf-`
- Toolchain: Arm GNU Toolchain 13.3.Rel1 (aarch64-none-elf-gcc 13.3.1,
  GNU ld/objcopy 2.42.0) on Windows 11.
- Result: all 24 translation units compiled with zero warnings; link and
  objcopy succeeded. Artifacts: `zenin-kernel.elf` 153,088 bytes,
  `zenin-kernel.bin` 73,960 bytes.
- Deviation note: the historical toolchain was `aarch64-linux-gnu-gcc` in
  WSL. WSL is unavailable in this session (service disabled; requires
  elevation). `CROSS_COMPILE` is overridable in the Makefile by design; the
  toolchain difference is recorded here per QEMU Evidence Requirements.

## QEMU

- QEMU 11.1.0 (`qemu-system-aarch64.exe`, winget
  SoftwareFreedomConservancy.QEMU).
- Machine `virt`, `gic-version=2`, CPU `cortex-a53`, SMP 2, memory 2048 MiB,
  kernel loaded as ELF (`-kernel kernel\zenin-kernel.elf`), serial captured
  to `kernel/qemu.log` (fresh run 2026-10-13; supersedes the stale
  2026-10-02 log).

## Results (all fresh, current tree)

| Area | Result |
|---|---|
| ARM64 kernel boot/smoke | PASS (EL1, identity MMU active) |
| PMM | PASS (aligned/contiguous alloc, bounds, free accounting) |
| MMU | PASS (noncontiguous pages, 2 MiB boundary, TLB, unmap identity, reclaim) |
| Heap basic | PASS (alignment, invalid/double free, large VM-backed alloc/free) |
| Undersized-hole regression | PASS (skip for 3-page request, reuse for 2-page request, live data + byte accounting intact) |
| Sustained VA reuse | PASS (64 cycles at same VA, guard live) |
| VA-window saturation/boundary | PASS (exact exhaustion, earliest + window-boundary reuse, full recovery) |
| Fragmented physical backing | PASS (six classes, 32 × 4 KiB objects, 900 KiB object) |
| Handle heap | PASS (fill/overflow/free 131071 slots, compaction, stale handles, 256 handles, mixed churn) |
| Z-Bus | PASS (payload, FIFO, full/empty, wraparound, pointer identity) |
| Input | PASS |
| NEON | PASS (128-case scalar equivalence) |
| System context | PASS |
| PMM accounting / idle budget | 129 pages / 516 KiB + image/BSS/stack 2,544 KiB = 3,060 KiB; QEMU budget check PASS (< 5 MiB) |
| Timer wake | PASS (one timer IRQ, INTID 27, woke from WFI) — fixed via GICC_CTLR.AckCtl |

## Allocator Test G (wraparound) status

The two-pass search in `vm_find_free_run` always considers the entire
window. The reachable boundary behavior is exercised by the saturation
regression (cursor wrap at the window end; reuse of ranges before and at
the boundary). Analysis of the hint policy shows a free run starting below
the hint other than at the hint cannot arise through the allocator API, so
a state where pass 0 is empty and pass 1 must satisfy the request is
defense-in-depth rather than reachable; this analysis is recorded, and the
executable evidence is the saturation/boundary regression.

## Timer root cause (KF002)

QEMU GICv2 model (`hw/intc/arm_gic.c`): a secure GICC_IAR/HPPIR read for a
Group 1 interrupt returns spurious 1022 unless `GICC_CTLR.AckCtl` is set.
Kernel programmed PPI 27 as Group 1 with `GICC_CTLR = 2` (no AckCtl), so
the PPI pended (`GICD_ISPENDR0 = 0x08000000`) and woke WFI but could never
be acknowledged. Fix: `GICC_CTLR = EnableGrp1 | AckCtl` in
`kernel/arch/aarch64/irq.c`. Classification: genuine kernel GIC programming
bug. Regression test: existing `zenin_irq_test_wfi` (failed before, passes
after; retained permanently).

---

# Verification Record — 2026-10-13 (Goals 1.6/1.7 work)

## Goal 1.6 accounting semantics (fresh run)

The idle-RAM report now follows the goal's exact counting rules:

```text
[zenin-memory] Kernel image code+rodata+data: 72 KiB; BSS: 2404 KiB; boot stack: 64 KiB
[zenin-memory] PMM-accounted resident pages: 129 pages / 516 KiB
[zenin-memory] OS-owned resident total (image+BSS+stack+PMM pages; shared allocations counted once): 3060 KiB
[zenin-memory] Platform-reserved (reported separately, not OS-owned): 512 KiB below kernel load address; device MMIO regions are outside the RAM window
[zenin-memory] QEMU OS-accounted idle-RAM budget: PASS (<5 MiB); actual-device measurement requires target hardware
```

Shared allocations counted once: static image/BSS/stack counted once via
linker bounds; every PMM page counted once via PMM counters; the PMM-managed
range begins at `__kernel_end`, so image and PMM counts do not overlap.
Hardware leg BLOCKED (host AMD64; no ARM target attached).

## Goal 1.7 installable image (fresh, reproducible)

- Pipeline: `python scripts/build_os_image.py` (deterministic; no
  timestamps/absolute paths embedded).
- Artifacts: `dist/zenin-os.elf` (bootable ELF system image),
  `dist/zenin-os.img` (installable flat image; load contract 0x40080000),
  `dist/zenin-os-image.manifest` (component inventory + sizes + SHA-256).
- Bootability VERIFIED on both artifacts: each booted under QEMU virt
  (`-kernel dist/zenin-os.elf` and `-kernel dist/zenin-os.img`) and passed
  the complete Phase 1 suite with zero FAILs (fresh runs, serial logs
  retained in session scratchpad).
- Reproducibility VERIFIED: two independent `make -B` clean rebuilds +
  image assemblies produced byte-identical artifacts:
  `zenin-os.elf` SHA-256 8379622232ded07018abb89c9d2800712e6f0995fa727c0505a0e813a0eebace,
  `zenin-os.img` SHA-256 7040dd5193ec5d4399f687df5ad5638177cb65d19e69e868090b5d073dc1aedc,
  manifest SHA-256 d6c224ad1881c86bbaf097231a5c09f377e35705d0f56494c39c0579697c2c67.
- Completeness: every linked object must be claimed by exactly one required
  component class (kernel, base system, AI services + shipped
  models/weights, bundled runtimes, verification) or the pipeline fails.
- Measurement: installable image size 73,960 bytes (0.07 MiB);
  requirement ≤ 300 MiB: PASS.
- Not demonstrated: storage-installer / boot-ROM / UEFI install path;
  shipping-quality models/weights and runtimes (current components are
  prototypes). Goal 1.7 therefore remains PARTIALLY VERIFIED.

---

# Verification Record — 2026-10-14 (D010 zero-fragmentation + UEFI image)

## Goal 1.5 — D010 fragmentation-proof heap (current tree)

Implementation: `kernel/mm/kheap.c` (sorted allocated-span list; free space
is implicit gaps), `kernel/arch/aarch64/mmu.c` (48-bit VA, TCR T0SZ=16,
lazy four-level heap tables with cascade reclamation), window constants in
`kernel/include/arch/aarch64/mmu.h`. Property and theorem: DECISIONS.md D010.

Fresh QEMU evidence (current tree, two consecutive full-suite runs green,
`qemu_d010d.log` / `qemu_d010e.log`):

- D010 window guarantee printed and asserted at test boot:
  "blocking all max-size gaps needs 786432 allocated pages vs 523283 RAM
  pages; fragmentation cannot cause allocation failure."
- D010 adversarial regression PASS: free-every-other churn over 32 × 8-page
  objects (16 holes), oversize 12-page request succeeded without
  overlapping any live allocation, a fitting request reused a freed hole,
  live data and byte/PMM accounting intact.
- All prior regressions PASS unchanged on the new allocator: undersized-hole
  skip/reuse, 64-cycle sustained VA reuse, six size classes + 32 × 4 KiB
  objects under fragmented physical backing, 900 KiB fragmented object
  (invalid/interior/double-free rejection is now fault-free by construction:
  kfree validates the page mapping before touching header memory), handle
  heap suite, Z-Bus, input, NEON, context, accounting.
- Idle accounting after the change: OS-owned resident 4,024 KiB
  (BSS grew for slab descriptors; window-size bitmaps removed), QEMU budget
  PASS (< 5 MiB).
- A regression found and fixed during bring-up: L2 page-table reclamation
  was gated on the parent-L1 identity check (leaked empty L2 pages);
  fixed and re-verified ("PMM reclaim verified" PASS).

## Goal 1.7 — UEFI install path (current tree)

Pipeline: `scripts/build_os_image.py` (deterministic PE32+ writer +
FAT16 assembly with scrubbed directory timestamps) and `scripts/efi_boot.c`
(BOOTAA64.EFI stub: loads ZENIN/OS.IMG at 0x40080000, exits boot services,
cleans caches, disables MMU, jumps to the kernel).

- Installable artifact: `dist/zenin-os-uefi.img` (FAT16 superfloppy:
  \EFI\BOOT\BOOTAA64.EFI + \ZENIN\OS.IMG), 64 MiB, ≤ 300 MiB PASS.
- Boot VERIFIED on ARM64 UEFI firmware (QEMU AAVMF `edk2-aarch64-code.fd`,
  virtio-blk disk): firmware loads BOOTAA64.EFI, the stub loads the kernel,
  and the COMPLETE Phase 1 suite passes with zero FAILs
  (`qemu_uefi6.log` / final run `qemu_uefi_final.log`), including the D010
  regression and timer wake (INTID 27).
- Reproducibility VERIFIED: two full pipeline runs produce byte-identical
  artifacts (kernel ELF, flat image, BOOTAA64.EFI, UEFI disk, manifest).
- Bring-up fixes applied to the stub (verified against the failures they
  caused): LoadedImage DeviceHandle offset (24), HandleProtocol instead of
  OpenProtocol, GetInfo-based file sizing, TX-full poll polarity, and an
  inline-asm register clobber in the kernel jump.

## Goal 1.6 — updated status

Idle accounting is verified on two boot paths (platform kernel loader and
UEFI), with goal-exact counting semantics. The mandatory actual-target-
hardware measurement remains BLOCKED: host is AMD64, no ARM target is
attached (USB/JTAG/serial scan negative), and UAC/WSL access restored the
build route but provides no ARM hardware.