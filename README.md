# Project Zenith (Universal Modular Android OS)

**Project Zenith** is a lightweight, high-performance, modular distribution of Android designed to run across diverse hardware targets via the **Project Treble GSI** architecture. 

It strips away heavy legacy Android framework overhead and replaces core system components with lightweight, modular alternatives while maintaining full compatibility with the Android ecosystem.

---

## 🏗️ Architecture Blueprint

```
┌────────────────────────────────────────────────────────┐
│             Zenith User Experience & UI                │
│  - Zenith System UI (Lightweight status bar & quick qs)│
│  - Zenith Core Shell / Launcher                        │
│  - Native Privacy & Permission Controller              │
├────────────────────────────────────────────────────────┤
│           Zenith Modular Framework & Services          │
│  - MicroG / UnifiedNLP Core Location Subsystem         │
│  - Modular DEX / Services Overrides (services.jar)     │
│  - Rust/C++ Lightweight System Daemons                 │
├────────────────────────────────────────────────────────┤
│                   Android Treble HAL                   │
│          AIDL / HIDL Hardware Interfaces               │
├────────────────────────────────────────────────────────┤
│             Vendor & Kernel (Target Devices)           │
│  - Out-of-tree Custom Kernel Modules (LKM)             │
│  - Vendor Hardware Blobs & Proprietary Drivers         │
└────────────────────────────────────────────────────────┘
```

---

## 🎯 Core Design Goals

1. **Ultra-Low Memory Footprint:** Run fluidly on constrained hardware (2GB/3GB RAM devices) by trimming heavy Java background services.
2. **True Universal GSI Target:** One system image bootable on Treble-compliant devices across Qualcomm, MediaTek, Exynos, and Unisoc.
3. **Modular Subsystems:** Independent build modules for kernel, native HAL shims, and system services without requiring full 400GB monolithic AOSP builds.
4. **De-Googled by Default:** Clean open-source foundation with optional microG support for banking and push notifications.

---

## 📁 Repository Layout

```
├── .github/
│   └── workflows/          # Automated Cloud CI/CD builds (GitHub Actions)
├── overlay/                # Runtime Resource Overlays (RROs) for system styling
│   ├── framework-res/      # Core framework resource tweaks
│   └── SystemUI/           # Minimalist status bar and QS icons
├── system/                 # Modular system components
│   ├── apps/               # Curated lightweight system apps
│   ├── bin/                # Custom native utilities and optimization scripts
│   ├── etc/                # System configs, permission xmls, sysconfig
│   └── framework/          # Services patches and custom framework extensions
├── tools/                  # Build, unpack, repack, and signing toolchain
│   ├── kitchen.py          # Universal GSI extraction and packaging engine
│   └── requirements.txt    # Toolchain dependencies
├── docs/                   # Architecture specs and hardware porting guides
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites
- Python 3.9+
- Git
- Android platform-tools (`adb`, `fastboot`)
- 7-Zip or `simg2img` / `make_ext4fs` toolchain
