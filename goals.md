# Project Zenin: The Superpowered AI-Native Universal OS

# Agent Interpretation Rules

This document defines the intended goals and acceptance targets of Project Zenin.

It is an authoritative project-goals document. Agents must preserve the goals rather than silently weakening, redefining, or replacing them for implementation convenience.

## How to interpret this document

### 1. Goals are requirements, not suggestions

A goal marked incomplete must remain incomplete until its acceptance criteria are actually satisfied.

Do not mark a goal complete because:

- code exists,
- the implementation appears correct,
- compilation succeeds,
- a partial test passes,
- an old test passed,
- an emulator-only approximation passes,
- or a previous agent claimed completion.

### 2. Quantitative requirements are literal

Numerical targets in this document are hard requirements unless an explicit architectural decision changes them.

Examples include:

- exactly zero OS-added latency,
- 0.0% idle CPU usage,
- 0.0% idle active CPU power contribution where specified,
- <5 MiB resident OS memory,
- ≤300 MiB complete installable OS image.

Do not silently reinterpret:

```text
zero
```

as:

```text very low
```

or:

```text bounded
```

or:

```text effectively negligible
```

Likewise, do not replace an exact measurement requirement with a theoretical argument.

### 3. Verification is part of the goal

A goal is not complete merely because its implementation exists.

Completion requires the evidence demanded by the goal itself.

When a goal explicitly requires measurement, artifact production, hardware execution, exhaustive inventory, or a direct-path comparison, those requirements are mandatory.

### 4. Distinguish implementation from verification

Use these states internally:

- **SPECIFIED** — defined by the project goals.
- **IMPLEMENTED** — relevant implementation exists.
- **VERIFIED** — current implementation satisfies the relevant acceptance test.
- **PARTIALLY VERIFIED** — some acceptance criteria pass while others remain open.
- **IMPLEMENTED BUT UNVERIFIED** — code exists but required evidence is missing.
- **BLOCKED** — verification/implementation cannot currently proceed because of an external dependency.
- **FAILED** — current evidence demonstrates that the requirement is not satisfied.

Never convert IMPLEMENTED or PARTIALLY VERIFIED into VERIFIED without evidence.

### 5. Current source outranks historical claims

Historical reports, old QEMU logs, previous agent statements, and old build artifacts describe previous states.

They may be retained as evidence of historical progress, but they do not prove the current source tree remains correct after modifications.

When source code changes, rerun the affected verification.

### 6. Do not weaken a goal to make an implementation pass

If an implementation cannot satisfy a requirement, do not modify the goal merely to make the implementation appear complete.

Instead:

1. identify the conflict,
2. document it,
3. determine whether the requirement or implementation is wrong,
4. make an explicit architectural decision if the requirement genuinely needs changing.

### 7. Adversarial interpretation

Foundational goals must be interpreted against adversarial workloads, not merely happy-path examples.

For memory/resource-management goals, consider:

- fragmentation,
- reuse,
- exhaustion,
- wraparound,
- repeated allocation/free cycles,
- live-resource preservation,
- boundary conditions,
- and pathological search patterns.

### 8. Agent responsibility

Before modifying a subsystem, an agent should inspect:

- `SYSTEM_SPEC.md`
- `ARCHITECTURE.md`
- `INVARIANTS.md`
- `DESIGN_RULES.md`
- `DECISIONS.md`
- `CURRENT_STATE.md`
- `KNOWN_FAILURES.md`
- `VERIFICATION.md`

when those files exist and are relevant.

The agent must understand the existing architecture and current state before redesigning a subsystem.

### 9. Phase gates

Later-phase functionality must not be used as evidence that earlier-phase requirements are satisfied.

A phase is complete only when its own acceptance criteria have been demonstrated.

### 10. Evidence integrity

Never fabricate:

- test results,
- benchmark numbers,
- hardware measurements,
- QEMU results,
- memory measurements,
- latency measurements,
- compatibility results,
- or successful builds.

When verification is unavailable, explicitly report the requirement as unverified or blocked.

# Goal Status Rules

The checkbox state in this file represents the project's accepted completion state.

An unchecked goal must remain unchecked until its acceptance criteria are satisfied.

For goals with multiple acceptance requirements, a goal remains unchecked if any mandatory requirement is incomplete.

When a goal becomes verified:

1. record the verification procedure in `VERIFICATION.md`;
2. record the implementation state in `CURRENT_STATE.md`;
3. remove or resolve the corresponding entry in `KNOWN_FAILURES.md`;
4. update `Achieved Features.md` with the demonstrated result;
5. only then change the goal checkbox to `[x]`.

If evidence later becomes invalid because the implementation changed, revert the checkbox to `[ ]` and document the regression.

Do not modify a goal's wording merely to accommodate an implementation.

**Core Philosophy:** Engineered from the bare-metal ground up. Ultra-clean, zero legacy bloat, lightning fast, with peak battery efficiency and extreme gaming performance that breathes new life and "superpowers" into any device—even 1.0 GHz potato hardware. The project has a hard target of exactly zero latency added by the OS itself across every OS component; latency intrinsic to hardware or other software is outside that OS-added-latency metric. This target applies in every phase and must be measured rather than assumed.

---

## ⚡ Core Identity & Tenets

1. **True Ground-Up Native Core:**
   - Not an Android fork or a generic Linux desktop skin.
   - Purpose-built bare-metal kernel architecture designed for instant responsiveness and zero wasted cycles.
2. **"Superpowers" on Potato Hardware:**
   - Revive budget, constrained, or legacy hardware (1.0 GHz CPU, 2 GB RAM).
   - Instant app launches, zero micro-stutter, and intelligent power gating that turns multi-hour battery life into multi-day battery life.
3. **Peak Battery Performance (True Zero Idle Drain):**
   - **Idle Battery Consumption = 0.0%**: Complete silicon sleep state via hardware `wfi` (Wait-For-Interrupt) and dynamic clock gating. When the screen is off and no task is active, CPU cores draw zero active milliwatts.
4. **Extreme Gaming Performance & Deterministic Latency:**
   - Real-time priority rendering with direct Vulkan/DRM frame presentation.
   - No background garbage collection int...erruptions, zero frame drops, and maximum GPU/CPU thermal headroom for continuous high FPS.
5. **AI-Native from the Silicon Up:**
   - On-device edge intelligence embedded into the system event loop.
   - Proactive context awareness with zero background telemetry or cloud lag.
6. **Ruthless Resource Efficiency:**
   - **Idle RAM:** Strictly < 5 MiB for all resident OS code and services with no user applications running.
   - **Installed OS Size:** Strictly ≤ 300 MiB, including the kernel, base system, AI services, shipped models/weights, and bundled runtimes.
   - **CPU Usage at Idle:** 0.0% (Zero busy-wait polling loops).
7. **Universal Application Interoperability (Integrated on Demand):**
   - Clean-room on-demand runtimes for Android (`.apk`), Windows (`.exe`), Linux (`ELF`), and macOS (`Mach-O`).
   - Zero background overhead when not actively running an application.

---

## 🏗️ Technical Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Zenin Fluid Neural UI & Shell                        │
│     (Direct hardware DRM/KMS compositor, sub-millisecond response)     │
├───────────────────┬───────────────────┬────────────────────────────────┤
│   Android Apps    │   Windows Apps    │          macOS Tools           │
│      (.apk)       │      (.exe)       │            (Mach-O)            │
│  (Waydroid/LXC)   │  (WINE / Box64)   │       (Darling / GNUstep)      │
├───────────────────┴───────────────────┴────────────────────────────────┤
│                     Zenin Native System ABI & IPC                      │
│             Zero-copy Z-Bus event mesh & capability broker             │
├────────────────────────────────────────────────────────────────────────┤
│                 Embedded Edge Neural Engine (GGML/NEON)                │
│          Direct hardware-accelerated INT2/INT4 edge inference          │
├────────────────────────────────────────────────────────────────────────┤
│            Zenin Microkernel / Hardware Abstraction (HAL)              │
│       - Bare-metal ARM64 boot & exception vector table                 │
│       - Physical memory page manager (PMM) & dynamic heap              │
│       - Zero-drain power gating (WFI / DVFS clock scaling)             │
│       - Universal Treble / DRM/KMS hardware driver interface           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 📋 Comprehensive Project Goal List

### Phase 1: Bare-Metal Ground-Up Foundation (The Superpower Engine)
**Zero-OS-latency requirement for this phase:** boot-to-kernel handoff, exception entry, UART services, physical-page allocation/free, and kernel-heap allocation/free must add exactly zero latency relative to their corresponding direct operation with no OS path. Instrument each path and report measured OS-added time and timer resolution; do not substitute fast/constant-time for zero.
- [x] **Goal 1.1:** Setup ARM64 cross-compilers (`aarch64-linux-gnu-gcc`) & QEMU bare-metal emulator.
- [x] **Goal 1.2:** Write the bare-metal ARM64 assembly entry point (`boot.S`) and linker script (`link.ld`).
- [x] **Goal 1.3:** Implement memory-mapped UART serial driver for instant early boot debugging.
- [x] **Goal 1.4:** Implement Physical Memory Manager (PMM) with 4KB bitmap page tracking (Only 16KB overhead for 512MB RAM!).
- [x] **Goal 1.5:** Implement dynamic kernel heap (`kmalloc` / `kfree`) with deterministic zero-fragmentation allocator.
  *(VERIFIED 2026-10-15: the D010 fragmentation-proof heap passes the full suite — undersized-hole reuse, 64-cycle reuse, D010 guarantee (floor(W/n_max)=786432 > RAM pages), six size classes + fragmented backing, 900 KiB VM-backed. The earlier span-corruption hang is root-caused and fixed (D013 TLB invalidation on VA reuse; see KF006). The phase-header "exactly zero OS-added latency" is a separate cross-phase requirement tracked as KF005 — currently at the QEMU measurement floor and needing a real cycle-accurate target to resolve below the timer bracket.)*
- [ ] **Goal 1.6:** Keep total OS-owned resident RAM below 5 MiB at idle, with no user applications running. Measure all resident kernel and system-service memory on actual target hardware; count shared allocations once and report platform-reserved memory separately. QEMU allocator accounting alone does not satisfy this goal.
- [ ] **Goal 1.7:** Keep the complete installable OS image at or below 300 MiB, including the kernel, base system, AI services, shipped models/weights, and bundled runtimes. Produce and measure a reproducible bootable image with an artifact manifest; measuring only the kernel binary or source repository does not satisfy this goal.
  *(Kernel + UEFI pipeline build clean and boot to a green suite; the full installable-image verification — manifest enumeration, ≤300 MiB measurement, and the byte-identical reproducibility check across two pipeline runs — is the remaining step. Not yet re-run against the post-fix tree this session.)*

### Phase 2: Power Architecture & Event-Driven IPC (Zero-Idle Battery & Gaming Mode)
**Zero-OS-latency requirement for this phase:** scheduler decisions, interrupt/kernel entry and exit, locks, power-state requests and wake paths, Z-Bus/IPC, system calls, display submission, and event delivery must add exactly zero latency relative to the same operation without the OS path. Measure each component and its end-to-end path; a bounded or low-latency result does not satisfy zero.
- [ ] **Goal 2.1:** Implement interrupt-driven WFI (Wait-For-Interrupt) sleep mode: CPU sleeps at 0.0% when idle.
- [ ] **Goal 2.2:** Dynamic Voltage and Frequency Scaling (DVFS): Scale down CPU to lowest micro-watt state when screen is off.
- [ ] **Goal 2.3:** Game Mode / Real-Time Scheduler: Lock UI and game render loops to hardware VSync with zero GC stutter.
- [ ] **Goal 2.4:** Build the high-speed Z-Bus zero-copy event loop connecting interrupts to system services.
- [ ] **Goal 2.5:** Universal Display Pipeline: Direct DRM/KMS framebuffer renderer (60fps/120fps smooth rendering on 1.0 GHz).
- [ ] **Goal 2.6:** Add exactly zero latency through the OS itself across network, display, input, CPU/GPU, sensors, audio, Bluetooth, and every other device path. Attribute and measure the OS portion separately from hardware and non-OS software delay. The acceptance target for OS-added time is exactly zero on every path and workload; retain raw measurements, timer resolution, and the direct-path comparison. Any positive measured OS contribution means this goal is not met.

### Phase 3: AI-First Intelligence Layer
**Zero-OS-latency requirement for this phase:** context reads, intent dispatch, model invocation, AI memory management, and AI-to-kernel actions must add exactly zero latency through OS code and services. Measure OS-attributed time separately from model computation and other non-OS work.
- [ ] **Goal 3.1:** Embed quantized neural engine (ARM NEON SIMD accelerated) directly into the OS core.
- [ ] **Goal 3.2:** System Context Awareness: Instant read access to battery, network, active window, and system state.
- [ ] **Goal 3.3:** Natural Language & Predictive Intent Dispatcher: Hardware actions execute directly without app layers.

### Phase 4: On-Demand Universal Multi-OS Subsystems
**Zero-OS-latency requirement for this phase:** executable loading, ABI translation, container setup, compatibility services, and runtime-to-kernel transitions must add exactly zero latency through OS code. Measure and report the OS portion independently for each supported workload.
- [ ] **Goal 4.1:** Android APK support via on-demand lightweight container (Waydroid/libhybris).
- [ ] **Goal 4.2:** Windows `.exe` execution via WINE + Box64 JIT compilation.
- [ ] **Goal 4.3:** macOS Mach-O command-line & clean-room GUI support via Darling + GNUstep.

### Phase 5: Universal Processor Compatibility
**Zero-OS-latency requirement for this phase:** architecture selection, HAL dispatch, platform initialization, and processor-specific kernel paths must add exactly zero latency through OS code on every supported processor and platform. Per-target measurements are required; portability or a successful build alone is insufficient.
- [ ] **Goal 5.1:** Make Project Zenin compatible with every processor type that has existed or exists as of 2026-10-04. Build an exhaustive processor/ISA inventory rather than defining a short supported subset. For every inventory entry, provide a native boot and hardware-abstraction port or a verified instruction-translation path, including the required memory, interrupt, timer, console, and device interfaces, and run the same applicable kernel capability tests. A shared C source tree, successful cross-compilation, or CPU-name detection alone does not count as compatibility. Keep this goal incomplete while any processor type in the exhaustive inventory lacks a verified execution path.
- [ ] **Goal 5.2:** Make the OS portable across device models and hardware platforms, with working support for each platform's processors, memory map, interrupt/timer controllers, network/display/input, GPU, sensors, audio, Bluetooth, and all other present device classes. Maintain an explicit platform/device inventory and verify real boot, driver operation, and capability tests on each entry; a generic HAL interface or emulator-only result does not establish portability.
