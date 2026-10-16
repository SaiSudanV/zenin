# Project Zenin

Project Zenin is an experimental bare-metal operating-system project. The current kernel implementation and verified boot path target **AArch64 on QEMU `virt`**. It is not currently a universal Android Treble GSI, and the repository does not yet provide a bootable kernel for other processor architectures.

The long-term portability goal is universal: support every processor type that has existed or exists now, and portable operation across device models with their hardware interfaces and peripherals. The OS also targets less than 5 MiB idle resident RAM, an installed size at or below 300 MiB including bundled AI assets, and **exactly zero latency added by OS code itself** across every component and device path. This strict latency target is not currently met or measured in full; it is distinct from time intrinsic to hardware or other software. These are **unachieved project goals**, not current compatibility or performance claims. Each architecture and platform needs working boot, memory and interrupt handling, drivers, and execution tests. See [the goals checklist](docs/goals.md) and [the achieved-features report](Achieved%20Features.md) for evidence and current gaps.

## Current prototype

- Bare-metal AArch64 entry point, linker script, UART output, and bitmap PMM.
- A lazy page-mapped heap with six size classes from 128 to 4096 bytes, plus a large-object path up to 256 MiB. A page-occupancy bitmap reuses freed virtual ranges while other allocations stay live; a QEMU regression check passed 64 repeated 5,000-byte allocate/free cycles at the same virtual address. Under fragmented PMM backing, 32 simultaneous 4000-byte allocations used frames with a longest contiguous run of one page. A 900 KiB allocation used 226 frames with a longest physical run of 126 pages. Size classes still waste some bytes internally, descriptor capacity and the virtual address range are finite, and virtual contiguity can fragment.
- A separate experimental 2 MiB handle-addressed heap can compact movable objects without invalidating handles. QEMU filled and freed all 131,071 16-byte-minimum handle slots, rejected overflow, reclaimed its measured 1,024 backing pages, and passed fragmentation/data-preservation and mixed-size churn checks. Raw-pointer clients are not migrated, compaction has measured nonzero cost, and the API is currently single-core only.
- Bounded SPSC Z-Bus queue with pointer handoff.
- AArch64 baseline-NEON first-layer classifier path; scalar-equivalence smoke check passes, model quality remains unknown.
- Partial context reporting for allocator memory and the active parser surface; battery, CPU frequency, and network readings are unavailable.
- QEMU smoke checks for selected PMM, heap, Z-Bus, and input operations.
- AArch64 EL1 MMU bootstrap enables a QEMU `virt` identity map for the configured 2 GiB RAM and low peripheral window. Slab and large-object allocations use contiguous virtual ranges backed by separately allocated physical pages in a 1 GiB page-mapped window; freed ranges can be reused. QEMU passed mapping, data, unmap/reclaim, noncontiguous-page, bounds, duplicate-map, all-size-class, large-allocation, and virtual-range-reuse checks. This memory map is not portable, and the allocator has not been verified on physical hardware.
- Experimental timer/GIC path; timer-driven WFI wake has **not** passed its QEMU check.

The resource targets are strict idle OS-owned resident RAM below 5 MiB and installed OS size at or below 300 MiB, including shipped AI services, models/weights, and runtimes. After freeing all test allocations, the latest QEMU run accounted for 3,068 KiB of kernel image+BSS+stack plus PMM pages, excluding platform reservations. The emulator budget check passed, but this is not an actual-device resident-memory measurement. The current kernel binary measured 82,328 bytes; a complete installable OS image has not been built or measured. Allocator operations have measured nonzero cost, and timer-counter diagnostics do not isolate exact OS-only latency. The exact-zero OS-added-latency target applies to all phases and remains unmet. The QEMU timer wake check also remains failed (GIC interrupt ID 1022).

## Build and run the current AArch64 prototype

In a Linux or WSL environment with `aarch64-linux-gnu-gcc`, GNU binutils, `make`, and `qemu-system-aarch64` installed:

```sh
cd kernel
make -B
qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a53 -smp 2 -m 2048 \
  -nographic -kernel zenin-kernel.bin
```

The kernel currently hardcodes the QEMU `virt` memory map and PL011 UART address. This command is an emulator test, not evidence of support for physical devices or other processor architectures.

## Repository layout

```text
kernel/                 Bare-metal kernel prototype
  arch/aarch64/         Current boot, vectors, timer/GIC, and linker code
  drivers/              Software framebuffer and input prototypes
  ipc/                  Z-Bus implementation
  mm/                   PMM and kernel heap
docs/goals.md           Project goals, including universal processor support
Achieved Features.md    Evidence-based progress against those goals
docs/processor-support.md Current, explicitly incomplete processor evidence inventory
```
