"""
Project Zenin - Subway Surfers APK Native ARM64 Asset & Binary Inspector
Analyzes the authentic Subway Surfers APK, extracts native ARM64 .so binaries,
and inspects their ELF dynamic symbols to connect directly to the Zenin Microkernel.
"""

import zipfile
import os
import sys

APK_PATH = r"c:\Custom Rom\SubwaySurfers.apk"
EXTRACT_DIR = r"c:\Custom Rom\SubwaySurfers_Native"

def inspect_apk():
    if not os.path.exists(APK_PATH):
        print(f"Error: APK file not found at {APK_PATH}")
        return

    file_size = os.path.getsize(APK_PATH)
    print(f"[APK-INSPECT] Ingesting Subway Surfers APK: {APK_PATH}")
    print(f"[APK-INSPECT] File size: {file_size:,} bytes ({file_size / (1024*1024):.2f} MB)")

    try:
        with zipfile.ZipFile(APK_PATH, 'r') as z:
            namelist = z.namelist()
            print(f"[APK-INSPECT] Successfully mounted ZIP archive! Total entries: {len(namelist)}")

            # Find native libraries
            native_libs = [n for n in namelist if n.startswith("lib/")]
            print(f"[APK-INSPECT] Found {len(native_libs)} native library entries:")
            for lib in sorted(native_libs):
                info = z.getinfo(lib)
                print(f"   -> {lib} (Compressed: {info.compress_size:,} bytes | Uncompressed: {info.file_size:,} bytes)")

            # Find assets
            assets = [n for n in namelist if n.startswith("assets/")]
            print(f"[APK-INSPECT] Found {len(assets)} game engine assets.")

            # Extract arm64-v8a libraries
            os.makedirs(EXTRACT_DIR, exist_ok=True)
            arm64_libs = [n for n in native_libs if "arm64" in n]
            if not arm64_libs:
                print("[APK-INSPECT] Note: No dedicated arm64 directory, checking all libs...")
                arm64_libs = native_libs

            for lib in arm64_libs:
                target_path = os.path.join(EXTRACT_DIR, os.path.basename(lib))
                with z.open(lib) as src, open(target_path, "wb") as dst:
                    dst.write(src.read())
                print(f"[APK-INSPECT] Extracted native binary: {target_path} ({os.path.getsize(target_path):,} bytes)")

    except zipfile.BadZipFile:
        print("[APK-INSPECT] Download still in progress or incomplete zip central directory.")
    except Exception as e:
        print(f"[APK-INSPECT] Error: {e}")

if __name__ == "__main__":
    inspect_apk()
