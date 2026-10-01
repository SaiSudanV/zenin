CROSS_COMPILE ?= aarch64-linux-gnu-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a53
LDFLAGS = -T arch/aarch64/link.ld -nostdlib

SRCS = arch/aarch64/boot.S sys/kmain.c mm/pmm.c mm/kheap.c ipc/zbus.c drivers/fb.c ai/intent_engine.c sys/compat_launcher.c
OBJS = arch/aarch64/boot.o sys/kmain.o mm/pmm.o mm/kheap.o ipc/zbus.o drivers/fb.o ai/intent_engine.o sys/compat_launcher.o

all: zenin-kernel.elf zenin-kernel.bin

arch/aarch64/boot.o: arch/aarch64/boot.S
	$(CC) $(CFLAGS) -c $< -o $@

sys/kmain.o: sys/kmain.c
	$(CC) $(CFLAGS) -c $< -o $@

mm/pmm.o: mm/pmm.c
	$(CC) $(CFLAGS) -c $< -o $@

mm/kheap.o: mm/kheap.c
	$(CC) $(CFLAGS) -c $< -o $@

ipc/zbus.o: ipc/zbus.c
	$(CC) $(CFLAGS) -c $< -o $@

drivers/fb.o: drivers/fb.c
	$(CC) $(CFLAGS) -c $< -o $@

ai/intent_engine.o: ai/intent_engine.c
	$(CC) $(CFLAGS) -c $< -o $@

sys/compat_launcher.o: sys/compat_launcher.c
	$(CC) $(CFLAGS) -c $< -o $@

zenin-kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

zenin-kernel.bin: zenin-kernel.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) zenin-kernel.elf zenin-kernel.bin
