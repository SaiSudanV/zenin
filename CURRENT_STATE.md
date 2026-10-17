# Current System State

> This document describes the implementation state at the time of its last verified update.
> It must be updated when implementation or verification state changes.

## Overall Status

**Phase 1: GOALS 1.5, 1.6, AND 1.7 ACHIEVED AND VERIFIED (2026-10-17);
phase-wide zero-latency requirement remains failed-as-measured (KF005).**

Goals 1.1–1.4 previously established.

Goal 1.5 achieved via the D010 fragmentation-proof heap: fragmentation can
never cause allocation failure (window sizing theorem, compile-time asserted
and adversarially tested), freed ranges instantly reusable, deterministic
bounded search.

Goal 1.7 achieved: reproducible installable UEFI-bootable image
(`dist/zenin-os-uefi.img`, 64 MiB ≤ 300 MiB) with manifest; boots on ARM64
UEFI firmware and passes the complete suite. Stream B: `64.00 MiB` logical
installable image / `0.31 MiB` sparse artifact, reproducible across clean
rebuilds.

Goal 1.6 achieved: `4868 KiB` OS-owned resident at idle under AAVMF
(**< 5 MiB PASS**; shared counted once, platform-reserved separate).
Stream A on real Ampere ARM64 hardware: `PMCCNTR` \(\Delta = 0\) across the
WFI idle window PASS. Test 9 (W⊕X split-subtree freeze) PASS. See
VERIFICATION.md (2026-10-16/17 record) for figures and evidence.

---

## Implemented / Previously Verified

The kernel has demonstrated functionality in:

- ARM64 boot/execution
- physical memory management
- PMM accounting
- MMU/virtual-memory mapping
- kernel heap allocation
- large-object virtual-address reuse
- handle heap
- Z-Bus
- input
- NEON
- allocator stress/regression tests

---

## Allocator Current State (updated 2026-10-13)

The heap tracks virtual-address occupancy/free regions and supports reuse of
released large-object ranges. The search hint preserves the earliest skipped
undersized hole so later smaller requests can reuse it.

**VERIFIED (fresh, current tree, 2026-10-13, `kernel/qemu.log`):**

- Undersized-hole/wraparound regression: PASS (skip-for-larger, reuse-for-
  smaller, live data intact, byte accounting exact).
- Sustained VA reuse (64 cycles with live guard): PASS.
- VA-window saturation regression (new): PASS — 1 GiB window filled to
  genuine exhaustion; earliest and window-boundary released ranges reused;
  full recovery with exact PMM/byte accounting.
- Fragmented-physical-backing workloads (six size classes, 32 simultaneous
  4 KiB objects, 900 KiB large object): PASS.
- Full Phase 1 suite: PASS.

Structural note (analysis of `vm_find_free_run` + hint writers): with the
current hint policy (release lowers hint to the freed page; successful
searches either end just past the allocated run or rewind to the earliest
skipped hole), a free run starting below the hint that differs from the hint
itself cannot arise, so the two-pass wraparound segment is defense-in-depth;
the saturation regression exercises the window-end cursor wrap
(hint = N → 0) and cross-boundary reuse that are reachable.

---

## Timer Wake (updated 2026-10-13)

**RESOLVED.** Root cause: `GICC_CTLR.AckCtl` (bit 2) was not set, so the
secure GICC view hid the Group 1 timer PPI behind spurious ID 1022.
Fixed in `kernel/arch/aarch64/irq.c` (GICC_CTLR = EnableGrp1 | AckCtl).
Fresh QEMU run: one timer IRQ (INTID 27) woke the core from WFI — PASS.
Verified on QEMU virt GICv2 only; physical GIC hardware unverified.

---

## Goal 1.6 Accounting (updated 2026-10-13)

The idle-RAM accounting report now matches the goal's exact measurement
semantics (fresh run): OS-owned resident total with shared allocations
counted once = **3,060 KiB** (code+rodata+data 72 KiB, BSS 2,404 KiB, boot
stack 64 KiB, PMM-accounted resident pages 129 / 516 KiB); platform-reserved
memory is reported separately (512 KiB below the kernel load address; device
MMIO outside the RAM window); QEMU budget check PASS (< 5 MiB).

The mandatory hardware measurement is now **VERIFIED**: `PMCCNTR`
\(\Delta = 0\) across the WFI idle window on real Ampere ARM64 hardware
(Stream A, 2026-10-16), and `4868 KiB` OS-owned resident at idle under
AAVMF on real aarch64 CPUs (**< 5 MiB PASS**). See VERIFICATION.md
(2026-10-16/17 record).

---

## Goal 1.7 Image Artifact (updated 2026-10-13)

Reproducible installable-image pipeline implemented
(`scripts/build_os_image.py`) and verified:

- `dist/zenin-os.elf` (bootable ELF system image) and `dist/zenin-os.img`
  (installable flat image, load contract 0x40080000) — both boot in QEMU
  virt and pass the complete Phase 1 suite (fresh runs).
- Reproducibility VERIFIED: two independent clean rebuilds produced
  byte-identical artifacts (SHA-256 in `dist/zenin-os-image.manifest`).
- `dist/zenin-os-image.manifest` enumerates every required component class
  (kernel, base system, AI services + shipped models/weights, bundled
  runtimes, verification) with per-component footprints and hashes; a
  completeness check fails the build if any linked object is unclaimed.
- Measured installable image size: 73,960 bytes (0.07 MiB) — ≤ 300 MiB PASS.

Remaining gaps for Goal 1.7 completion: storage-installer/boot-ROM/UEFI
install path (only the platform kernel-loader boot path is verified), and
prototype maturity of shipped models/weights and bundled runtimes.

---

## Evidence Classification

### VERIFIED

Only facts supported by fresh execution against the current source tree.

### HISTORICAL

Results obtained from earlier source states or isolated copies.

### IMPLEMENTED BUT UNVERIFIED

Code exists, but the relevant current-tree verification has not yet been completed.

### KNOWN FAILURE

A current or reproducible test demonstrates incorrect behavior.

---

## Current Build / Run Route (2026-10-13)

- Build: `make -C kernel CROSS_COMPILE=aarch64-none-elf-` with Arm GNU
  Toolchain 13.3.Rel1 (clean build, zero warnings).
- Run: QEMU 11.1.0, `virt`, `gic-version=2`, `cortex-a53`, `smp 2`,
  `m 2048`, kernel loaded as ELF; serial log captured to `kernel/qemu.log`.
- Note: historical route was `aarch64-linux-gnu-gcc` under WSL; WSL is
  unavailable in this session (see `KNOWN_FAILURES.md`).

---

## Next Required Work

1. Phase 1 goals 1.5/1.6/1.7 are VERIFIED (see Overall Status); no
   hardware-measurement gap remains.
2. KF005: the phase-wide exact-zero OS-added latency requirement remains
   failed as measured; its baseline wording needs an explicit architectural
   clarification, or paired direct-baseline instrumentation must be built
   and the paths reduced to baseline-equivalent instruction sequences.
3. Mature the shipped models/weights and bundled runtimes (Phase 3/4
   content) inside the now-established image pipeline.
4. Record fresh evidence as it is produced; Phase 1 is complete only when
   every mandatory acceptance criterion — including the phase-wide
   zero-latency requirement and Goal 1.6 hardware measurement — is
   satisfied.
