#!/usr/bin/env bash
set -e

WORKSPACE="/mnt/c/Custom Rom"
BUILD_DIR="$WORKSPACE/build"
ROOTFS_DIR="$BUILD_DIR/rootfs"
INITRAMFS_OUT="$BUILD_DIR/initramfs-zenin.cpio.gz"
CROSS_LIB="/usr/aarch64-linux-gnu/lib"

echo "====================================================="
echo "   Building Project Zenin Layer 7 Micro-RootFS       "
echo "   Target Architecture: AArch64 (ARM64)             "
echo "   Footprint Constraint: < 15 MB Compressed CPIO     "
echo "====================================================="

rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"/{bin,sbin,lib,lib64,usr/bin,usr/lib,proc,sys,dev,compat/linux,compat/android,compat/windows,compat/macos}

# 1. Copy required AArch64 dynamic loader & C runtime from local toolchain
echo "[*] Installing minimal AArch64 C runtime & dynamic linker..."
cp "$CROSS_LIB/ld-linux-aarch64.so.1" "$ROOTFS_DIR/lib/"
cp "$CROSS_LIB/libc.so.6" "$ROOTFS_DIR/lib/"
cp "$CROSS_LIB/libm.so.6" "$ROOTFS_DIR/lib/"
cp "$CROSS_LIB/libdl.so.2" "$ROOTFS_DIR/lib/" 2>/dev/null || true
cp "$CROSS_LIB/libpthread.so.0" "$ROOTFS_DIR/lib/" 2>/dev/null || true

# Symlink standard dynamic linkers
cd "$ROOTFS_DIR/lib64"
ln -sf ../lib/ld-linux-aarch64.so.1 ld-linux-aarch64.so.1

# 2. Compile Zenin Native Micro-Init (PID 1)
echo "[*] Compiling Zenin Native Micro-Init (core/init.c)..."
aarch64-linux-gnu-gcc -O2 -static "$WORKSPACE/core/init.c" -o "$ROOTFS_DIR/init"
chmod +x "$ROOTFS_DIR/init"

# 3. Create a test native userland binary (Hello world from Linux ELF)
cat << 'EOF' > "$BUILD_DIR/test_app.c"
#include <stdio.h>
int main(void) {
    printf("[userland-app] Running native AArch64 Linux userland binary!\n");
    return 0;
}
EOF
aarch64-linux-gnu-gcc -O2 "$BUILD_DIR/test_app.c" -o "$ROOTFS_DIR/bin/test_app"
chmod +x "$ROOTFS_DIR/bin/test_app"

# 4. Pack into lightweight bootable Initramfs (CPIO format)
echo "[*] Packaging into compressed Initramfs..."
cd "$ROOTFS_DIR"
find . -print0 | cpio --null --create --format=newc | gzip -9 > "$INITRAMFS_OUT"

# 5. Measure output sizes
UNCOMPRESSED_SIZE=$(du -sh "$ROOTFS_DIR" | cut -f1)
INITRAMFS_SIZE=$(ls -lh "$INITRAMFS_OUT" | awk '{print $5}')

echo "====================================================="
echo "   Layer 7 Micro-RootFS Packaging Complete!          "
echo "   Uncompressed RootFS Size: $UNCOMPRESSED_SIZE       "
echo "   Compressed Initramfs Size: $INITRAMFS_SIZE        "
echo "   Initramfs Archive: $INITRAMFS_OUT                 "
echo "====================================================="
