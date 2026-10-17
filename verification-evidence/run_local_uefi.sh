#!/usr/bin/env bash
set -euo pipefail
cd "/mnt/c/Custom Rom/_gic-ram-verification"
source_revision=${1:?pass the committed source revision}

python3 scripts/build_os_image.py \
  --toolchain-prefix=aarch64-linux-gnu- \
  --size-tool=aarch64-linux-gnu-size \
  --objdump=aarch64-linux-gnu-objdump \
  --source-revision="$source_revision" \
  > verification-evidence/image-6.log 2>&1

cmp kernel/zenin-kernel.elf dist/zenin-os.elf
cmp kernel/zenin-kernel.bin dist/zenin-os.img
extract_dir=$(mktemp -d /tmp/zenin-os-extract.XXXXXX)
mcopy -i dist/zenin-os-uefi.img ::/ZENIN/OS.IMG "$extract_dir/OS.IMG"
cmp kernel/zenin-kernel.bin "$extract_dir/OS.IMG"
rm -r "$extract_dir"
sha256sum kernel/arch/aarch64/mmu.c kernel/arch/aarch64/mmu.o \
  kernel/zenin-kernel.elf kernel/zenin-kernel.bin \
  dist/zenin-os-uefi.img > verification-evidence/image-hashes-6.txt

rm -f verification-evidence/uefi-boot-5.log verification-evidence/qemu-5.log
qemu-system-aarch64 \
  -M virt -cpu max -m 3072 \
  -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
  -drive file=dist/zenin-os-uefi.img,format=raw,if=virtio \
  -display none -serial file:verification-evidence/uefi-boot-5.log \
  >verification-evidence/qemu-5.log 2>&1 &
qemu_pid=$!
for ((i = 0; i < 90; i++)); do
  if grep -aq "PASS: one timer IRQ" verification-evidence/uefi-boot-5.log 2>/dev/null &&
     grep -aq "OS-owned resident total:" verification-evidence/uefi-boot-5.log 2>/dev/null &&
     grep -aFq "[zenin-test] COMPLETE:" verification-evidence/uefi-boot-5.log 2>/dev/null; then
    break
  fi
  if ! kill -0 "$qemu_pid" 2>/dev/null; then break; fi
  sleep 1
done
kill "$qemu_pid" 2>/dev/null || true
qemu_status=0
wait "$qemu_pid" 2>/dev/null || qemu_status=$?
echo "QEMU exit after capture: $qemu_status"
grep -aE 'UEFI firmware \(version|zenin-mmu\] GICD/GICC|zenin-irq|Test 9/9|text RX|zenin-test\] COMPLETE|Accounting inputs|OS-owned resident total|Platform-reserved|idle-RAM budget' \
  verification-evidence/uefi-boot-5.log || true
