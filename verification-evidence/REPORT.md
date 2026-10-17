# GIC and idle-RAM verification evidence

## Target and source history

The recovery branch starts at `dd5b6074727de580f8a18a052f6315ee6fa70b36`. The original `main` worktree was preserved at `1702edf262f63e1ea747f8b4dbe2157bc9ce6440` with 1,663 pre-existing changed or untracked paths. All changes and generated verification evidence in this directory are isolated to `_gic-ram-verification`.

## Findings behind the recovery

The original hosted Actions run checked out the reported fix but `make` said “Nothing to be done for 'all'.” The tracked `kernel/arch/aarch64/mmu.o` was not rebuilt, so that run did not establish that `dd5b607`'s source fix was compiled into the kernel. The revised workflow records pre-clean source/object hashes and timestamps, runs the kernel Makefile's clean target, traces every compile/link step, and hashes the outputs. It byte-compares the resulting ELF and flat binary to the packaged outputs, then extracts the FAT `OS.IMG` and compares it to the same flat binary.

The previous local boot log reported `18014398509481856 KiB` of platform-reserved memory. The subtraction was `kernel_start - selected_ram_base`; UEFI reported selected PMM base `0x400A0000` above kernel start `0x40080000`, so unsigned subtraction wrapped. The corrected code reports this span as unavailable and keeps it out of the OS-owned total rather than clamping it.

The boot descriptor capacity was also too small for the observed AAVMF map. It now accepts up to 512 RAM and MMIO descriptors, and the UEFI stub rejects truncation. Descriptor range validation and memory arithmetic have standalone host C tests.

## Local execution evidence (WSL x86_64, QEMU 11.1.0, AAVMF 2026.05-2)

These are QEMU AArch64 guest runs under software emulation on an x86_64 WSL host. They are not physical-device or ARM-host measurements. `uefi-boot-3.log` and `uefi-boot-4.log` preserve the full serial captures; corresponding QEMU stderr/stdout, clean build logs, image assembly logs, and SHA-256 manifests are kept beside this report.

Both completed captures show active GICD/GICC/UART descriptors with Device-nGnRnE attributes, GICv2 programmed, one timer IRQ waking WFI, W^X checks passing, and an OS-owned resident total of 4,876 KiB (kernel image 4,420 KiB + 114 PMM pages / 456 KiB), strictly below 5,120 KiB. Platform-reserved span below the kernel is explicitly unavailable for the observed map.

The complete kernel smoke suite reports FAIL because the PMM cold-path latency check observed +20 ticks for allocation and +10 ticks for free. This is retained as a failure; the focused GIC, W^X, and RAM assertions are separate. Other reported system metrics such as CPU utilization, power, IPC latency, and FPS were not measured.

`uefi-boot-1.log` and `uefi-boot-2.log` retain earlier failed/incomplete attempts: first the 64-range descriptor truncation; then a boot where a large UART framebuffer export delayed the timer marker. The capacity and ordering fixes followed those observations.

## Repeatability and final revision

`repeat_build.sh` and `run_local_uefi.sh` take the committed source revision as their argument, rebuild cleanly, regenerate the UEFI image manifest, compare packaged payloads, and capture boot output. Run both only after the final source commit; then record the revision and matching outputs here. The hosted workflow runs on an ARM64 GitHub runner but always executes the kernel as a QEMU/AAVMF guest. Direct physical hardware verification remains unproven until that artifact is booted on the target hardware.