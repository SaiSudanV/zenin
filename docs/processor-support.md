# Processor support inventory

**Status: starter inventory only; not exhaustive. Universal processor compatibility is not achieved.**

Goal 5.1 covers every processor type that existed or exists on 2026-10-04, including distinct ISA families, architecture revisions, and processor model families. A processor is supported only when Zenin has a native boot path or a verified instruction-translation path, working platform interfaces, and executed capability tests. Compiler acceptance and CPU identification do not count.

| Processor/ISA family | Current evidence | Boot/runtime status | Required next evidence |
| --- | --- | --- | --- |
| AArch64, QEMU `virt`, Cortex-A53 | Forced build and QEMU execution; PMM, heap, Z-Bus, input, NEON arithmetic, and partial context smoke checks passed | Prototype runs; timer IRQ wake fails; QEMU board assumptions are hardcoded | Fix timer routing; test real AArch64 board/model families and platform drivers |
| x86-64 | Partial `hal_cpu.h` instruction primitives; build target is explicitly rejected | No kernel boot/runtime, interrupt controller, timer, or device HAL | Add boot and platform port; execute on an x86-64 emulator and physical processor models |
| ARM 32-bit (A/R/M profiles) | QEMU ARM emulator is installed; no ARM32 kernel build or boot has been run | Unsupported | Establish separate profile-specific ports and run applicable tests on each profile/model family |
| RISC-V (32/64-bit and extensions) | No RISC-V emulator or cross-compiler was found in the checked WSL tool locations | Unsupported | Establish native boot/HAL for each required width/profile/extension set and execute tests |
| Other historical/current processor families | Not yet catalogued | Unsupported / not inventoried | Enumerate without omitting legacy, proprietary, DSP, MCU, and other CPU families; choose native ports or verified translation paths |

This table is an evidence snapshot, not the exhaustive inventory required by Goal 5.1. No processor family beyond the listed AArch64 QEMU target has a demonstrated Zenin boot. The available WSL tools checked on 2026-10-04 included AArch64 and ARM QEMU emulators plus an x86-64 GCC, but no x86-64 or RISC-V system emulator and no ARM32 cross-compiler. No external toolchain was installed for this check.
