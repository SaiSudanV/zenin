#!/usr/bin/env bash
set -e

WORKSPACE="/mnt/c/Custom Rom"
BUILD_DIR="$WORKSPACE/build"
ROOTFS_DIR="$BUILD_DIR/rootfs"
ALPINE_VERSION="3.21.3"
ALPINE_TAR="alpine-minirootfs-${ALPINE_VERSION}-aarch64.tar.gz"
ALPINE_URL="https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/aarch64/${ALPINE_TAR}"

echo "==============================================="
echo "   Building Project Zenin Phase 1 Rootfs       "
echo "   Target Architecture: ARM64 (aarch64)        "
echo "==============================================="

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 1. Download Alpine ARM64 Mini-Rootfs if not already cached
if [ ! -f "$ALPINE_TAR" ]; then
    echo "[*] Downloading official Alpine aarch64 mini-rootfs (~3.5 MB)..."
    curl -fsSL -o "$ALPINE_TAR" "$ALPINE_URL"
else
    echo "[*] Found cached Alpine mini-rootfs: $ALPINE_TAR"
fi

# 2. Extract cleanly
echo "[*] Extracting base rootfs into $ROOTFS_DIR..."
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"
tar -xzf "$ALPINE_TAR" -C "$ROOTFS_DIR"

# 3. Compile Zenin Native Micro-Init for ARM64
echo "[*] Compiling Zenin Native Micro-Init (core/init.c)..."
aarch64-linux-gnu-gcc -O2 -static "$WORKSPACE/core/init.c" -o "$ROOTFS_DIR/sbin/zenin-init"
chmod +x "$ROOTFS_DIR/sbin/zenin-init"

# 4. Link PID 1 /init directly to Zenin Micro-Init
echo "[*] Setting /sbin/zenin-init as default PID 1 init..."
ln -sf /sbin/zenin-init "$ROOTFS_DIR/init"

# 5. Measure Disk Footprint
DISK_USAGE=$(du -sh "$ROOTFS_DIR" | cut -f1)
echo "==============================================="
echo "   Base Rootfs Built Successfully!             "
echo "   Uncompressed Disk Usage: $DISK_USAGE         "
echo "==============================================="
