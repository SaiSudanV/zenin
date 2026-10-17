#!/usr/bin/env bash
set -euo pipefail
cd "/mnt/c/Custom Rom/_gic-ram-verification"
source_revision=${1:?pass the committed source revision}

gcc -std=c11 -Wall -Wextra -Werror scripts/test_memory_accounting.c \
  -o /tmp/zenin-memory-tests
/tmp/zenin-memory-tests
make -C kernel clean CROSS_COMPILE=aarch64-linux-gnu-
make --trace -C kernel CROSS_COMPILE=aarch64-linux-gnu- \
  > verification-evidence/build-6.log 2>&1
sha256sum kernel/arch/aarch64/mmu.c kernel/arch/aarch64/mmu.o \
  kernel/zenin-kernel.elf kernel/zenin-kernel.bin \
  > verification-evidence/postbuild-6.txt
diff -u verification-evidence/postbuild-4.txt \
  verification-evidence/postbuild-6.txt

python3 scripts/build_os_image.py \
  --toolchain-prefix=aarch64-linux-gnu- \
  --size-tool=aarch64-linux-gnu-size \
  --objdump=aarch64-linux-gnu-objdump \
  --source-revision="$source_revision" \
  > verification-evidence/image-6.log 2>&1

extract_dir=$(mktemp -d /tmp/zenin-os-repeat.XXXXXX)
trap 'rm -r "$extract_dir"' EXIT
cmp kernel/zenin-kernel.elf dist/zenin-os.elf
cmp kernel/zenin-kernel.bin dist/zenin-os.img
mcopy -i dist/zenin-os-uefi.img ::/ZENIN/OS.IMG "$extract_dir/OS.IMG"
cmp kernel/zenin-kernel.bin "$extract_dir/OS.IMG"
sha256sum kernel/arch/aarch64/mmu.c kernel/arch/aarch64/mmu.o \
  kernel/zenin-kernel.elf kernel/zenin-kernel.bin \
  dist/zenin-os-uefi.img > verification-evidence/image-hashes-6.txt
diff -u verification-evidence/image-hashes-4.txt \
  verification-evidence/image-hashes-6.txt
echo "CLEAN BUILD AND UEFI IMAGE: BYTE-IDENTICAL TO FIRST BUILD"
