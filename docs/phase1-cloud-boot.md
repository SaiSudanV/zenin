# Phase 1 — Cloud ARM64 Boot Handoff (Oracle Ampere A1 / AWS Graviton)

Goal 1.6 ("OS-owned resident RAM at idle on actual target hardware") is
authoritatively measured by booting the Zenin UEFI image on **real ARM64
silicon** and reading the idle-RAM line from the serial console. The GitHub
Actions `uefi-boot` job gives an automated AAVMF capture (real Ampere CPU via
KVM, but QEMU's virt machine); this document is the **non-QEMU** secondary
capture on a real cloud VM.

## Artifact

| File | Purpose |
|------|---------|
| `dist/zenin-os-uefi.img` | 64 MiB FAT16 superfloppy, UEFI-bootable (`EFI/BOOT/BOOTAA64.EFI` + `ZENIN/OS.IMG`). Sparse on NTFS (~0.31 MiB on disk); logical bytes + SHA-256 unchanged. |
| `dist/zenin-os-image.manifest` | Per-component sizes + SHA-256; `installable-image-size` must be ≤ 300 MiB. |

Verify the artifact before shipping it:

```bash
python3 scripts/build_os_image.py --toolchain-prefix= --size-tool=size --objdump=objdump
sha256sum dist/zenin-os-uefi.img     # must match the manifest's uefi-image sha256
```

## What the boot proves

On a successful boot the serial console must show, in order:

1. `[zenin-boot]` — `boot_info` accepted (firmware `GetMemoryMap` → kernel).
2. `[zenin-mmu] Kernel map frozen: static tables RO, heap subtree XNTable-locked.` — the W⊕X split-subtree freeze (D012).
3. `[Test 9/9] W^X mapping split and split-subtree freeze (D012)... [PASS]`.
4. `[zenin-memory] OS-owned resident total (image+BSS+stack+PMM pages; shared allocations counted once): <N> KiB` — **this `<N>` is the Goal 1.6 figure**; it must be `< 5120` (5 MiB).

## Booting on Oracle Cloud A1 (Ampere)

Oracle A1 VMs are AArch64 with a UEFI firmware. Two practical routes:

**A. Attach the image as a boot volume / block device.** Import
`zenin-os-uefi.img` as a block volume (or bake it into a custom image), attach it
to an A1 instance, and select it as the boot device. The AMI/UEFI firmware loads
`EFI/BOOT/BOOTAA64.EFI` from the FAT superfloppy. Read the serial console from
the instance's **Console History** / serial console (OCI supports serial
console over SSH for AArch64).

**B. Natively on a UEFI-capable A1 while stopped.** Copy the image to the
instance's boot disk, write it to a spare block device (`dd
if=zenin-os-uefi.img of=/dev/sdb bs=4M conv=fsync`), then boot from it via the
firmware boot menu (press ESC at the UEFI prompt and select the FAT disk).

## Booting on AWS Graviton (EC2)

EC2 Graviton instances (`t4g`/`m6g`/`c7g`/…) are AArch64 Nitro. Nitro's UEFI
loads `EFI/BOOT/BOOTAA64.EFI`. Use the image as an AMI root volume (import via
`vmimport` / EC2 Image Builder) or attach it as a secondary block device and
boot from it. Read output via the **EC2 serial console**
(`aws ec2 get-console-output`, or the EC2 serial console SSH session):

```bash
aws ec2 get-console-output --instance-id i-xxxxxxxx --latest \
  --output text --query Output
```

## Expected result

The captured console output contains the Goal 1.6 figure:

```
[zenin-memory] OS-owned resident total (image+BSS+stack+PMM pages; shared allocations counted once): <N> KiB
```

Record `<N>` as the authoritative Goal 1.6 idle-RAM measurement. It must be
`< 5120` KiB (5 MiB). That closes Phase 1 Goal 1.6 on actual target hardware.

## Why a secondary capture is required

Goal 1.6 states that QEMU allocator accounting alone does not satisfy the goal.
The GitHub Actions `uefi-boot` job runs on real Ampere silicon but still under
QEMU's virt machine (KVM), so its memory map is virtualized. Booting the same
artifact on a bare cloud VM (Oracle A1 / Graviton) exercises the real firmware
`GetMemoryMap` and the real physical RAM layout, which is the definitive
measurement the goal asks for.
