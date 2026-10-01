# Project Zenin: Official Architecture & System Goals

**Classification:** Core System Specification  
**Architecture:** AI-First Hybrid Universal Operating System  
**Hardware Baseline:** Low-end ARM/x86 (1.0 GHz CPU, 2 GB RAM, eMMC Storage)  

---

## 🎯 Hard Performance & Resource Targets

| Metric | Target (Strict) | Worst-Case Ceiling | Standard Android Baseline |
| :--- | :--- | :--- | :--- |
| **Idle OS RAM Usage** | **≤ 200 MB** | 500 MB max under peak load | ~1.4 GB – 1.8 GB |
| **Total OS Image Size** | **≤ 500 MB** | 1.0 GB max (including prebuilts) | ~3.5 GB – 6.0 GB |
| **CPU Efficiency Target** | **Full speed on 1.0 GHz CPU** | Minimal background daemon polling | Constant background wake-locks |
| **Available Headroom** | **≥ 1.5 GB RAM free** | Dedicated for Edge AI models & user apps | 0 MB free on 2GB devices |

---

## 🏛️ The Hybrid Architecture Blueprint: Integrating Existing Proven Tech

Instead of reinventing wheels from scratch, Zenin integrates established, production-grade open-source components into a unified lightweight stack:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   Zenin AI-Integrated UI & Shell                       │
│    (Fluid touch interface + embedded ambient AI proactive agent)       │
├───────────────────┬───────────────────┬────────────────────────────────┤
│   Android Apps    │   Linux Apps      │     Windows Apps (.exe)        │
│      (.apk)       │  (ELF / Flatpak)  │       (WINE / Proton)          │
│   (Waydroid LXC)  │  (Native Musl)    │      (Box64 / FEX-Emu)         │
├───────────────────┴───────────────────┴────────────────────────────────┤
│                 Zenin Native System Bus & Event Mesh                   │
│       - Event-driven, zero-polling IPC (zero CPU waste at idle)        │
│       - Instant state sharing between apps and AI agent                │
├────────────────────────────────────────────────────────────────────────┤
│           Zenin Embedded Edge AI Engine (llama.cpp / GGML)             │
│       - Sub-billion / 1B-2B INT2/INT4 quantized neural models          │
│       - Highly optimized ARM NEON / FP16 SIMD kernels (runs on 1GHz)   │
├────────────────────────────────────────────────────────────────────────┤
│         Base OS: Lightweight Linux Core (Alpine / Musl / BusyBox)      │
│       - Micro-Init (PID 1) + Direct DRM/KMS Framebuffer Compositor     │
├────────────────────────────────────────────────────────────────────────┤
│                 Universal Driver & Hardware Abstraction                │
│       - Project Treble HAL / libhybris (Universal phone compatibility) │
│       - Linux Mainline DRM/KMS & evdev (Tablets, PCs, SBCs)            │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 📋 Comprehensive Project Goal List

### Phase 1: The Ultra-Lightweight Base Foundation (Target: < 150 MB RAM, < 400 MB Disk)
- [x] **Goal 1.1:** Establish cross-compilation and automated local testing toolchain (ARM64 GCC + QEMU).
- [ ] **Goal 1.2:** Integrate a stripped, musl-based minimal Linux rootfs (Alpine/Toybox base) taking < 100 MB disk space.
- [ ] **Goal 1.3:** Build direct DRM/KMS hardware display output (bypassing heavy desktop managers and Android SurfaceFlinger).
- [ ] **Goal 1.4:** Enforce zero-polling power management: CPU sleeps when idle to guarantee full performance at 1.0 GHz.

### Phase 2: Embedded Edge AI Core (Runs on 1.0 GHz CPU & < 800 MB RAM)
- [ ] **Goal 2.1:** Integrate standalone `llama.cpp` / GGML embedded C++ engine with ARM NEON SIMD optimizations.
- [ ] **Goal 2.2:** Benchmark sub-billion parameter models (SmolLM-135M/360M, TinyLlama 1.1B, Qwen2.5-0.5B quantized to 2-bit/4-bit).
- [ ] **Goal 2.3:** Implement Deep System Context injection: The AI has direct, real-time read/write access to OS events, active screen state, notifications, and battery status.
- [ ] **Goal 2.4:** Build Intent Execution Engine: Natural language and automated background tasks route directly to OS system calls.

### Phase 3: Universal Multi-OS Application Compatibility
- [ ] **Goal 3.1: Android Apps (.apk):** Integrate lightweight containerized Android runtime (Waydroid/LXC) that only draws RAM on demand when an APK is launched.
- [ ] **Goal 3.2: Linux Apps (ELF / CLI / GUI):** Native out-of-the-box execution with Musl/Glibc compatibility layers.
- [ ] **Goal 3.3: Windows Apps (.exe):** Integrate ARM64 WINE + Box64/FEX translation layer for running standard Windows executables on ARM hardware.
- [ ] **Goal 3.4: macOS Mach-O Support:** Integrate Darling Mach-O translation subsystem for command-line utilities.

### Phase 4: Universal Hardware Deployment & Packaging
- [ ] **Goal 4.1: Project Treble GSI Target:** Package Zenin as a universal `system.img` flashable on any Project Treble Android device.
- [ ] **Goal 4.2: Generic ARM/x86 ISO/IMG:** Package bootable image for standard PCs, single-board computers (Raspberry Pi), and laptops.
- [ ] **Goal 4.3: Strict Resource Auditing:** Automated CI check ensuring the entire OS distribution image never exceeds 1.0 GB and idle memory never exceeds 200 MB.
