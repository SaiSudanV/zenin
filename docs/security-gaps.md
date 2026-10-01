# Project Zenin: Threat Model & Security Vulnerability Matrix
**Document ID:** SEC-GAP-001  
**Classification:** Future Security Update Gaps & Threat Mitigations  
**Scope:** Universal Android GSI, Unlocked Bootloader Threat Models, Hardware Root of Trust Limitations  

---

## Overview
This document tracks 41 known attack vectors and security gaps inherent to custom operating system distribution, bootloader state transitions, hardware-backed attestation failures, and vendor TEE trust models. 

Each threat is mapped to its failure mode, architectural defense strategy, and implementation roadmap for Project Zenin.

---

## A. Boot and Flash Attacks

| # | Attack Vector | Technical Impact | Architectural Defense & Mitigation | Implementation Status |
|---|---|---|---|---|
| 1 | **Evil-maid reflash** (`fastboot flash` of boot, system, vendor) | The device accepts and boots the trojaned image. If TEE binds keys to boot state, `/data` won't decrypt, but a fake lock screen can capture PIN/passphrase. | Relock with custom AVB key where supported. On unlocked: verify TEE key binding, use tamper seals, and enforce second-device attestation. | `Planned` |
| 2 | **`fastboot boot`** (RAM boot) | Attacker kernel runs in RAM with full hardware privileges; untouched OS boots on next restart leaving zero trace on disk. | Detect via TEE/secure element boot-session counter. Enforce auto-shutdown when left unattended. | `Under Research` |
| 3 | **Rollback to an old build** | Attacker flashes an older Zenin build with unpatched CVEs. No rollback index enforced when unlocked. | Implement monotonic version rollback counter inside TEE/StrongBox checked prior to FBE master key release. | `High Priority` |
| 4 | **Inactive A/B slot swap** | Attacker flashes malicious payload into inactive slot, then switches slot via `fastboot set_active`. | Verify signature integrity and hash state of both A/B slots and Virtual A/B snapshots prior to boot. | `Planned` |
| 5 | **Vendor-layer implant** (`vendor`, `vendor_boot`, `dtbo`) | Stealthy driver/kernel implant lives below the GSI, which normally never verifies vendor partitions. | Cryptographic verification of all reachable block partitions, not just `/system`. | `Planned` |
| 6 | **Pre-compromised second-hand phone** | Previous owner/reseller installed persistent firmware backdoors. | Mandatory full-reflash protocol of all reachable partitions + clear disclosure of unverified partitions. | `Documentation` |
| 7 | **Debug flags from the boot image** | Modified boot image sets `ro.debuggable=1`, root ADB, permissive SELinux, or disables `dm-verity`. | Treat boot image as untrusted. Chain verification from kernel to userland; enforce read-only runtime flags. | `Under Research` |
| 8 | **Persistent malware after runtime exploit** | Malware with root rewrites `/system` or `/boot` and survives reboot and factory reset. | Relocked AVB 2.0 where possible. On unlocked devices, mount dynamic partitions read-only with block-level verity checks. | `Ongoing` |
| 9 | **Loss of hypervisor/kernel hardening** (RKP, Knox, pKVM) | Custom kernel disables or skips vendor hypervisor monitors, simplifying privilege escalation. | Device-specific profile verification; retain vendor hypervisors where pKVM is available. | `Device Specific` |
| 10 | **Vendor `fastboot oem` commands** | Undocumented OEM commands allow arbitrary RAM reading or signature bypass. | Audit and document known vendor OEM commands per chipset; warn users of high-risk OEM bootloaders. | `Documentation` |
| 11 | **Low-level modes** (Qualcomm EDL, MediaTek BROM, Odin) | Proprietary test modes sit below the OS. Leaked loaders read/write raw flash and dump RAM. | Recommend hardware with cryptographically enforced secure boot and disabled test points. | `Hardware Dependent` |
| 12 | **Fault injection & BootROM bugs** (e.g. Kamran BROM) | Physical voltage/clock glitching defeats secure boot on silicon level regardless of lock state. | Require Secure Element (Titan M / StrongBox) hardware for high-assurance deployments. | `Hardware Dependent` |
| 13 | **Debug ports and diag modes** (UART, JTAG, Qualcomm Diag) | Hardware consoles accessible via test pads or USB modes. | Disable serial/diag drivers in kernel config and block via strict SELinux policies. | `Planned` |

---

## B. Data Extraction Attacks

| # | Attack Vector | Technical Impact | Architectural Defense & Mitigation | Implementation Status |
|---|---|---|---|---|
| 14 | **Forensic extraction** (Cellebrite / Graykey) | Attacker boots custom kernel while phone is in After-First-Unlock (AFU) state with keys in RAM. | Configurable auto-reboot timer after idle, strict USB data blocking when device is locked, Lockdown Mode. | `High Priority` |
| 15 | **RAM remanence on warm reboot** | Cold/warm reboot into attacker kernel recovers residual cryptographic keys from DRAM. | Zeroize RAM during shutdown; enforce cold power-off when unattended. | `Under Research` |
| 16 | **Clone or wipe `persist` & EFS/modem data** | IMEI, radio calibration, and DRM device certificates cloned or destroyed. | Enforce block-level read-only mount on NVRAM/persist partitions; backup critical calibration blobs. | `Planned` |
| 17 | **Software-only KeyMint** | No hardware rate-limiting. Offline brute-forcing of user credentials possible via kernel dump. | Check security level (`SOFTWARE` vs `TRUSTED_ENVIRONMENT` vs `STRONGBOX`). Prohibit high-value data on software-only. | `High Priority` |
| 18 | **Privileged kernel attacks the TEE** | Compromised kernel sends malformed SMC/HVC calls to Keymaster/Gatekeeper trusted apps. | Audit TEE interface drivers; isolate Keymaster communications; maintain up-to-date vendor trustlet firmware. | `Device Specific` |
| 19 | **Lockout / Biometric limits outside TEE** | Attacker kernel bypasses failed attempt counter and brute-forces lockscreen PIN. | Enforce credential attempt counters strictly inside TEE/Gatekeeper hardware. | `High Priority` |
| 20 | **Unlock wipe isn't a real erase** | OEM unlock process fails to physically overwrite flash blocks or clear secure storage. | Verify cryptographic master key deletion (crypto-shredding) during setup. | `Planned` |
| 21 | **Logs and diagnostics leak secrets** | Bug reports, tombstones, and `logcat` buffer contain PII, tokens, or cryptographic artifacts. | Scope, filter, and strip debug logging in release builds; disable memory core dumps. | `Planned` |
| 22 | **SD card & Adoptable storage** | Removable storage readable on foreign host if unencrypted. | Enforce device-bound AES-256-XTS encryption on all external storage media. | `Planned` |

---

## C. Ecosystem and Attestation Effects

| # | Attack / Effect | Technical Impact | Architectural Defense & Mitigation | Implementation Status |
|---|---|---|---|---|
| 23 | **Play Integrity failure** | Device fails Google Play Integrity checks; banking, government, and enterprise apps refuse to open. | Implement embedded Keystore Attestation Shim; fallback to software certificate chain; outreach to open standards. | `Active Focus` |
| 24 | **Attestation exposes unlocked state** | Remote servers detect bootloader state and track/fingerprint or discriminate against users. | Evaluate attestation response sanitization; balance privacy tradeoff against app compatibility. | `Under Research` |
| 25 | **Widevine DRM drops to L3** | Video streaming platforms downgrade playback resolution to 480p/540p. | Preserve OEM vendor DRM keys where possible; document per-device Widevine capabilities. | `Documentation` |
| 26 | **Vendor penalties** (Knox e-fuse) | Hardware fuse blown permanently on Samsung devices; Knox container disabled permanently. | Explicit pre-flash warnings and detection tool before bootloader unlock on Knox-equipped hardware. | `High Priority` |
| 27 | **Enterprise, ID, eSIM & Wallet break** | Carrier eSIM provisioning or national digital identity apps fail due to root-of-trust check. | Per-device certification testing matrix; support physical SIM fallback where eSIM TEE fails. | `Testing` |
| 28 | **Remote Key Provisioning without Google** | De-Googled OS cannot reach Google RKP servers to refresh hardware attestation keys. | Build local/alternative RKP key provider; document implications for Google-free builds. | `Under Research` |
| 29 | **Bank terms and liability** | Financial institutions deny fraud reimbursement on modified/unlocked firmware. | Clear user-facing documentation and legal disclaimer regarding local financial regulations. | `Documentation` |

---

## D. Supply Chain, Update and User-Side Attacks

| # | Attack Vector | Technical Impact | Architectural Defense & Mitigation | Implementation Status |
|---|---|---|---|---|
| 30 | **Trojaned lookalike Zenin builds** | Attacker distributes malicious builds using Zenin branding; bootloader boots them without check. | Deterministic reproducible builds, cryptographically published SHA-256 manifests, on-device verifier app. | `High Priority` |
| 31 | **Signing key theft** | Compromise of release signing key allows attacker to push signed malicious OTA updates. | Store release keys in offline Hardware Security Modules (YubiKey/HSM) with pre-planned revocation protocol. | `High Priority` |
| 32 | **Compromised flashing host or tool** | Malicious PC or web installer modifies images during transfer over USB. | Signed installation manifests verified locally by bootable installer before flashing blocks. | `Planned` |
| 33 | **Update channel attacks (MITM/Downgrade)** | Rogue OTA server redirects update URL or pushes vulnerable older version. | Enforce HTTPS certificate pinning, monotonic build version checks, and payload signature verification. | `High Priority` |
| 34 | **Clock rollback** | Resetting device clock makes expired certificates or revoked keys appear valid. | Monotonic hardware timestamp counter combined with cryptographically signed network time (NTS/Roughtime). | `Planned` |
| 35 | **Check-then-use gaps (TOCTOU)** | Attacker modifies partition blocks between verification pass and execution. | Enforce per-block on-the-fly verification via `dm-verity` Merkle trees rather than one-time pre-boot checks. | `Core Architecture` |
| 36 | **User roots Zenin** (Magisk / KernelSU) | User installs root, opening potential attack vectors for malicious apps. | Provide clear security toggles: user-controlled root management or strict root-free security profile. | `Design Choice` |
| 37 | **Social engineering of unlock/flash** | Malicious third party convinces user to unlock bootloader and flash malicious firmware. | Visual lock warnings, interactive safety guides, and unambiguous first-boot notifications. | `Documentation` |
| 38 | **Stale vendor firmware & kernels** | GSI updates `/system` but underlying vendor drivers and kernel remain unpatched. | Comprehensive on-device Patch Matrix showing exact patch levels of Kernel, Vendor, and System independently. | `High Priority` |
| 39 | **Theft resale and no FRP value** | Thief wipes device and resells it since Factory Reset Protection is weakened. | Investigate persistent hardware-anchored anti-theft flags where vendor TEE permits. | `Under Research` |
| 40 | **Closed-source blobs** | Proprietary vendor drivers, modem baseband, and TEE trustlets cannot be audited. | Isolate vendor HALs via strict AIDL IPC boundaries; document all trusted proprietary blobs. | `Core Architecture` |
| 41 | **Stale trust & no remote proof** | Stale ADB key authorizations survive; backend services cannot verify genuineness. | First-boot security reset; wipe stale authorizations; design cloud services with zero-trust principles. | `Planned` |

---

## 🎯 Implementation Roadmap by Priority

### Priority Tier 1: Immediate & High Impact
- **#17:** Software-only KeyMint detection and warning.
- **#14:** Auto-reboot after idle timer & USB lock during locked state.
- **#30:** Reproducible build manifests & hash verification.
- **#31:** Hardware-backed offline release signing keys (HSM).
- **#3:** Monotonic rollback counter in update engine.

### Priority Tier 2: Per-Device Hardware Verification
- **#1, #2, #15, #18, #19:** Hardware TEE binding tests, warm reboot DRAM zeroing, Gatekeeper brute-force protection.

### Priority Tier 3: Core Architecture & Platform Hardening
- **#35:** Uncompromising per-block `dm-verity` enforcement on all system partitions.
- **#40:** Strict SELinux `neverallow` rules isolating proprietary vendor blobs.
- **#38:** Multi-layer security patch reporting (System vs. Vendor vs. Kernel).
