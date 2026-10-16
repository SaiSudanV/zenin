# Tooling preferences

- Native Windows build route (WSL unavailable in this environment): Arm GNU Toolchain 13.3.Rel1 (aarch64-none-elf) at C:\toolchains\arm-gnu-aarch64, GNU make (ezwinports) via winget, QEMU 11.1.0 at "C:\Program Files\qemu". Confidence: 0.9
- Build command: `make -C kernel CROSS_COMPILE=aarch64-none-elf-` with PATH prepended by the toolchain bin dir and the make bin dir. Confidence: 0.9
- Run command: `qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a53 -smp 2 -m 2048 -display none -monitor none -serial file:<log> -kernel <elf-or-img>`. Confidence: 0.85
- Record any deviation from the historical toolchain (aarch64-linux-gnu-gcc under WSL) alongside the evidence in VERIFICATION.md. Confidence: 0.85
- Editing `link.ld` does not trigger a relink (make tracks only objects); force it with `make -B` or by deleting the ELF first. Confidence: 0.9
- GNU ld rejects `(NOBITS)` section-type syntax on output sections; use plain `.bss` for zero-disk-size state instead. Confidence: 0.85
