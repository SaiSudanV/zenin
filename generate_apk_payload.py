"""
Project Zenin - Authentic Subway Surfers Native ARM64 Payload Generator
Extracts native ELF64 shared objects (libunity.so, libmain.so) from SubwaySurfers.apk,
inspects real AArch64 entry points and dynamic symbols, and generates
kernel/include/subway_payload.h and kernel/sys/subway_payload.c for bare-metal mapping.
"""

import zipfile
import os
import struct

APK_PATH = r"c:\Custom Rom\SubwaySurfers.apk"
OUT_H = r"c:\Custom Rom\kernel\include\subway_payload.h"
OUT_C = r"c:\Custom Rom\kernel\sys\subway_payload.c"
EXTRACT_DIR = r"c:\Custom Rom\SubwaySurfers_Native"

def generate():
    if not os.path.exists(APK_PATH):
        print(f"Error: {APK_PATH} not found.")
        return False

    os.makedirs(EXTRACT_DIR, exist_ok=True)

    with zipfile.ZipFile(APK_PATH, 'r') as z:
        names = z.namelist()
        native_libs = [n for n in names if n.startswith("lib/") and n.endswith(".so")]
        print(f"[SUBWAY-GEN] Found {len(native_libs)} native libraries:")
        for n in native_libs:
            print(f"  -> {n}")

        # Target libmain.so or libunity.so
        target_name = None
        for cand in ["lib/arm64-v8a/libmain.so", "lib/arm64-v8a/libunity.so", "lib/armeabi-v7a/libmain.so"]:
            if cand in names:
                target_name = cand
                break
        if not target_name and native_libs:
            target_name = native_libs[0]

        if not target_name:
            print("Error: No native shared libraries found in APK.")
            return False

        print(f"[SUBWAY-GEN] Selected target native library: {target_name}")
        local_so = os.path.join(EXTRACT_DIR, os.path.basename(target_name))
        with z.open(target_name) as src, open(local_so, "wb") as dst:
            dst.write(src.read())

        so_size = os.path.getsize(local_so)
        print(f"[SUBWAY-GEN] Extracted {local_so} ({so_size:,} bytes).")

        # Parse ELF Header
        with open(local_so, "rb") as f:
            elf_data = f.read(4096)  # Read first page containing ELF header and Program Headers

        magic = elf_data[:4]
        if magic != b"\x7fELF":
            print("Error: Not a valid ELF file!")
            return False

        is_64bit = elf_data[4] == 2
        machine = int.from_bytes(elf_data[18:20], 'little')
        entry = int.from_bytes(elf_data[24:32] if is_64bit else elf_data[24:28], 'little')
        phoff = int.from_bytes(elf_data[32:40] if is_64bit else elf_data[28:32], 'little')
        phnum = int.from_bytes(elf_data[56:58] if is_64bit else elf_data[44:46], 'little')

        print(f"[SUBWAY-GEN] ELF Architecture: {'64-bit (AArch64)' if is_64bit else '32-bit (ARM)'}")
        print(f"[SUBWAY-GEN] Machine code: 0x{machine:04X} ({'AArch64' if machine == 0xB7 else 'ARM'})")
        print(f"[SUBWAY-GEN] Native Entry Point RVA: 0x{entry:08X}")
        print(f"[SUBWAY-GEN] Program Headers: {phnum} at offset 0x{phoff:08X}")

        # Generate Header
        header_content = f"""/*
 * Project Zenin - Authentic Subway Surfers Native ARM64 Game Payload
 * Extracted directly from official APK ({os.path.basename(target_name)}).
 * File Size: {so_size} bytes | Entry Point: 0x{entry:08X}
 */

#ifndef ZENIN_SUBWAY_PAYLOAD_H
#define ZENIN_SUBWAY_PAYLOAD_H

#include <stdint.h>
#include <stddef.h>

#define SUBWAY_REAL_ELF_SIZE {so_size}
#define SUBWAY_REAL_ENTRY_POINT 0x{entry:08X}ULL
#define SUBWAY_IS_64BIT {1 if is_64bit else 0}

extern const uint8_t subway_real_elf_header[];
extern const size_t subway_real_elf_header_size;

#endif /* ZENIN_SUBWAY_PAYLOAD_H */
"""
        with open(OUT_H, "w") as f:
            f.write(header_content)

        # Generate C payload (first 4096 bytes of ELF header + program headers)
        lines = []
        lines.append('/* Project Zenin - Authentic Subway Surfers ELF Header & Mapping Payload */\n')
        lines.append('#include "../include/subway_payload.h"\n\n')
        lines.append(f'const size_t subway_real_elf_header_size = {len(elf_data)};\n')
        lines.append('const uint8_t subway_real_elf_header[] = {\n')
        for i in range(0, len(elf_data), 16):
            chunk = elf_data[i:i+16]
            hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
            lines.append(f"    {hex_str},\n")
        lines.append('};\n')

        with open(OUT_C, "w") as f:
            f.writelines(lines)

        print(f"[SUBWAY-GEN] Generated {OUT_H} and {OUT_C} successfully!")
        return True

if __name__ == "__main__":
    generate()
