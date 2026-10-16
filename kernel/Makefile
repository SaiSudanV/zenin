ARCH ?= aarch64
ifneq ($(ARCH),aarch64)
$(error Unsupported ARCH='$(ARCH)': only the AArch64 kernel boot/runtime is implemented; do not treat other ISAs as compatible)
endif

CROSS_COMPILE ?= aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a53 -mgeneral-regs-only -fno-zero-initialized-in-bss
LDFLAGS = -T arch/aarch64/link.ld -nostdlib

SRCS = arch/aarch64/boot.S arch/aarch64/vectors.S arch/aarch64/irq.c arch/aarch64/mmu.c sys/kmain.c mm/pmm.c mm/kheap.c mm/kheap_handle.c ipc/zbus.c drivers/fb.c drivers/zgl.c drivers/input.c ui/compositor.c ai/intent_engine.c ai/neural_core.c sys/shell.c sys/compat_launcher.c sys/abi_shim.c sys/zpkg.c sys/zld.c sys/unified_bridge.c sys/production_test.c sys/container_loader.c sys/pe_loader.c
OBJS = arch/aarch64/boot.o arch/aarch64/vectors.o arch/aarch64/irq.o arch/aarch64/mmu.o sys/kmain.o mm/pmm.o mm/kheap.o mm/kheap_handle.o ipc/zbus.o drivers/fb.o drivers/zgl.o drivers/input.o ui/compositor.o ai/intent_engine.o ai/neural_core.o sys/shell.o sys/compat_launcher.o sys/abi_shim.o sys/zpkg.o sys/zld.o sys/unified_bridge.o sys/production_test.o sys/container_loader.o sys/pe_loader.o

all: zenin-kernel.elf zenin-kernel.bin

arch/aarch64/boot.o: arch/aarch64/boot.S
	$(CC) $(CFLAGS) -c $< -o $@

arch/aarch64/vectors.o: arch/aarch64/vectors.S
	$(CC) $(CFLAGS) -c $< -o $@

arch/aarch64/irq.o: arch/aarch64/irq.c
	$(CC) $(CFLAGS) -c $< -o $@

arch/aarch64/mmu.o: arch/aarch64/mmu.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/kmain.o: sys/kmain.c
	$(CC) $(CFLAGS) -c $< -o $@

mm/pmm.o: mm/pmm.c
	$(CC) $(CFLAGS) -c $< -o $@

mm/kheap.o: mm/kheap.c
	$(CC) $(CFLAGS) -c $< -o $@

mm/kheap_handle.o: mm/kheap_handle.c
	$(CC) $(CFLAGS) -c $< -o $@

ipc/zbus.o: ipc/zbus.c
	$(CC) $(CFLAGS) -c $< -o $@

drivers/fb.o: drivers/fb.c
	$(CC) $(CFLAGS) -c $< -o $@

drivers/zgl.o: drivers/zgl.c
	$(CC) $(CFLAGS) -c $< -o $@

drivers/input.o: drivers/input.c
	$(CC) $(CFLAGS) -c $< -o $@

ui/compositor.o: ui/compositor.c
	$(CC) $(CFLAGS) -c $< -o $@

ai/intent_engine.o: ai/intent_engine.c
	$(CC) $(CFLAGS) -c $< -o $@

ai/neural_core.o: ai/neural_core.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/shell.o: sys/shell.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/compat_launcher.o: sys/compat_launcher.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/abi_shim.o: sys/abi_shim.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/zpkg.o: sys/zpkg.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/zld.o: sys/zld.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/unified_bridge.o: sys/unified_bridge.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/production_test.o: sys/production_test.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/container_loader.o: sys/container_loader.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/pe_loader.o: sys/pe_loader.c
	$(CC) $(CFLAGS) -c $< -o $@

zenin-kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

zenin-kernel.bin: zenin-kernel.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) zenin-kernel.elf zenin-kernel.bin
