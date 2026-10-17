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

### Final committed revision and hosted run

Commit `b11b65abfa11d80447445f1bf6bbdb69c11e29d4` is a descendant of the reported `dd5b6074727de580f8a18a052f6315ee6fa70b36`. Its recovery branch was pushed and triggered Actions run [#25](https://github.com/SaiSudanV/zenin/actions/runs/39012719649), which completed successfully on `ubuntu-24.04-arm` using QEMU/AAVMF. The host architecture is ARM64; the kernel ran as a virtual guest, not directly on physical hardware.

The hosted run passed the C range/accounting tests, clean source build, payload identity comparisons, and boot assertions for active GIC/UART Device-nGnRnE descriptors, GICv2 programming, one timer IRQ waking WFI, W^X, and the strict RAM budget. Hosted OS-owned resident memory was 4,880 KiB: 4,424 KiB kernel image plus 114 PMM pages (456 KiB). The selected PMM range begins at `0x40099000`, above kernel start `0x40080000`, and the platform-reserved span is correctly unavailable. The full kernel smoke-suite marker is still `FAIL`; inspect the serial log for its failing PMM latency check.

The post-commit local repeat used the same revision and produced byte-identical clean-build kernel hashes and UEFI disk hash as the prior clean build. Hashes: `mmu.c` `a3de84aaa1e0c93c3684df08849014fa56e1b641eb5c7cf6ab3ca985705c98dc`; `mmu.o` `4bc1b3d0a2980547c9ae18831caf45bb95879532b498cc7ec192ae649a64a5ea`; ELF `ad1cf3b75db0b38013c4049cafa7e2b9840fb7cb451ac4153b3c1cf88b2c6553`; flat binary `a0e74c1e50648aae06cd5baa778abfb515d57180fcaaf5d0551ce67f722dafc9`; UEFI disk `fd7b3f7e63209e81d1ce6ac35cc4a4a571b2638bcd7e45733dbbe39c21b178e6`. See `postbuild-4.txt`/`postbuild-6.txt` and `image-hashes-4.txt`/`image-hashes-6.txt`.

The local post-commit run again showed the timer wake and 4,876 KiB; its full capture is `uefi-boot-5.log`. The hosted run's full build/serial logs and the packaged ELF/images are available in artifact `uefi-verification-39012719649-b11b65abfa11d80447445f1bf6bbdb69c11e29d4` (artifact SHA-256 `9c63ebb2c765416755607be352ec32e41230f4efee79f9af9b5ef671adc74ec1`).

### Final evidence state

- **VERIFIED in QEMU/AAVMF:** the committed GIC mappings are active as Device-nGnRnE; GICv2 programs; one architectural timer interrupt wakes WFI; W^X checks pass; OS-owned resident total remains below 5 MiB on both local x86_64-hosted emulation and ARM64-hosted QEMU.
- **VERIFIED repeatability:** clean builds and packaged UEFI images matched byte-for-byte across repeated local builds; hosted packaging comparisons also passed.
- **FAIL:** the broad kernel smoke suite reports a PMM cold-path latency regression. That check is visible and separate from the focused acceptance assertions.
- **UNPROVEN:** direct physical target hardware behavior and idle RAM under its real firmware/platform map. The hosted ARM64 runner still ran the kernel inside QEMU.
