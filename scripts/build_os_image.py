#!/usr/bin/env python3
"""Assemble the Project Zenin installable OS image and artifact manifest.

Reads the built kernel artifacts (kernel/zenin-kernel.elf, kernel/zenin-kernel.bin),
classifies the linked subsystems into the image's required component classes
(kernel, base system, AI services + shipped models/weights, bundled runtimes),
measures per-component footprints from the object files, and emits:

  dist/zenin-os.elf              bootable ELF system image (QEMU -kernel path)
  dist/zenin-os.img              installable flat image (load contract: 0x40080000)
  dist/zenin-os-image.manifest   deterministic artifact manifest

The output is reproducible: it embeds no timestamps and no absolute paths, so
rebuilding the same source tree with the same toolchain yields identical bytes.

Usage:
  python scripts/build_os_image.py [--objdump <path-to-aarch64-objdump>]
                                   [--size-tool <path-to-aarch64-size>]
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL_DIR = os.path.join(REPO_ROOT, "kernel")
DIST_DIR = os.path.join(REPO_ROOT, "dist")

LOAD_ADDRESS = 0x40080000
IMAGE_SIZE_LIMIT_BYTES = 300 * 1024 * 1024
UEFI_IMAGE_BASE = 0x10000000
UEFI_TEXT_RVA = 0x1000

# Component classes required by Goal 1.7's image definition.
COMPONENTS = [
    ("kernel-core", "kernel",
     ["arch/aarch64/boot.o", "arch/aarch64/vectors.o", "arch/aarch64/irq.o",
      "arch/aarch64/mmu.o", "sys/kmain.o", "mm/pmm.o", "mm/kheap.o",
      "mm/kheap_handle.o"],
     "ARM64 boot/entry/exceptions, PMM, MMU, kernel heap, handle heap"),
    ("base-system", "base system",
     ["sys/shell.o", "sys/zpkg.o", "sys/zld.o", "ui/compositor.o",
      "drivers/zgl.o", "drivers/fb.o", "drivers/input.o", "ipc/zbus.o",
      "sys/unified_bridge.o", "sys/abi_shim.o", "sys/compat_launcher.o"],
     "shell, Z-Bus, package/linker services, compositor/graphics, input, "
     "ABI/compat dispatch"),
    ("ai-services", "AI services + shipped models/weights",
     ["ai/neural_core.o", "ai/intent_engine.o"],
     "embedded neural engine and shipped prototype classifier weights "
     "(hand-authored; not a validated trained model)"),
    ("bundled-runtimes", "bundled runtimes",
     ["sys/container_loader.o", "sys/pe_loader.o"],
     "binary-format runtime subsystems (ELF/PE/Mach-O/APK loaders; prototype)"),
    ("verification", "verification",
     ["sys/production_test.o"],
     "in-kernel production/smoke test suite"),
]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git_revision():
    try:
        rev = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL).decode().strip()
        dirty = subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL).decode().strip()
        return rev + ("-dirty" if dirty else "")
    except Exception:
        return "unknown"


def tool_version(tool):
    try:
        out = subprocess.check_output([tool, "--version"], stderr=subprocess.STDOUT)
        return out.decode(errors="replace").splitlines()[0].strip()
    except Exception:
        return "unavailable"


def object_sizes(size_tool, obj_rel):
    """Return (text, data, bss) bytes for one object file."""
    obj_path = os.path.join(KERNEL_DIR, obj_rel)
    out = subprocess.check_output([size_tool, "-d", obj_path]).decode()
    line = out.strip().splitlines()[-1].split()
    return int(line[0]), int(line[1]), int(line[2])


def build_pe32_plus(text_bytes, entry_offset):
    """Wrap raw code in a deterministic PE32+ EFI application image.

    The stub is position-independent (ADRP/ADD), so a single dummy
    IMAGE_REL_BASED_ABSOLUTE relocation entry satisfies the PE loader's
    relocatability requirement as a no-op.
    """
    file_align = 0x200
    sect_align = 0x1000

    def align(v, a):
        return (v + a - 1) // a * a

    text_raw = text_bytes + b"\0" * (align(len(text_bytes), file_align) - len(text_bytes))
    reloc_payload = struct.pack("<IIHH", UEFI_TEXT_RVA, 12, 0x0000, 0x0000)
    reloc_raw = reloc_payload + b"\0" * (align(len(reloc_payload), file_align) - len(reloc_payload))
    reloc_rva = align(UEFI_TEXT_RVA + len(text_bytes), sect_align)

    dos = bytearray(0x80)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x80)

    pe = bytearray()
    pe += b"PE\0\0"
    pe += struct.pack("<HHIIIHH",
                      0xAA64,          # Machine: AArch64
                      2,               # NumberOfSections
                      0,               # TimeDateStamp (deterministic)
                      0, 0,            # symbol table
                      0xF0,            # SizeOfOptionalHeader (PE32+)
                      0x0022)          # EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
    size_of_headers = align(0x80 + 4 + 20 + 0xF0 + 2 * 40, file_align)
    opt = bytearray()
    opt += struct.pack("<HBB", 0x20B, 1, 0)       # Magic, linker ver
    opt += struct.pack("<IIII", len(text_raw), len(reloc_raw), 0,
                       UEFI_TEXT_RVA + entry_offset)  # sizes + entry RVA
    opt += struct.pack("<I", UEFI_TEXT_RVA)        # BaseOfCode
    opt += struct.pack("<Q", UEFI_IMAGE_BASE)      # ImageBase
    opt += struct.pack("<II", sect_align, file_align)
    opt += struct.pack("<HHHHHH", 0, 0, 0, 0, 0, 0)  # OS/image/subsystem ver
    opt += struct.pack("<I", 0)                    # Win32VersionValue
    opt += struct.pack("<I", reloc_rva + sect_align)  # SizeOfImage
    opt += struct.pack("<II", size_of_headers, 0)  # SizeOfHeaders, CheckSum
    opt += struct.pack("<HH", 10, 0)               # Subsystem=EFI_APP, DllChars
    opt += struct.pack("<QQQQ", 0x100000, 0x1000, 0x100000, 0x1000)
    opt += struct.pack("<II", 0, 16)               # LoaderFlags, dirs
    dirs = [(0, 0)] * 16
    dirs[5] = (reloc_rva, len(reloc_payload))      # Base relocation directory
    for rva, size in dirs:
        opt += struct.pack("<II", rva, size)
    assert len(opt) == 0xF0

    def section(name, vsize, rva, raw_size, raw_ptr, ch):
        s = name.ljust(8, b"\0")[:8]
        s += struct.pack("<IIIIIIHHI", vsize, rva, raw_size, raw_ptr,
                         0, 0, 0, 0, ch)
        return s

    pe += opt
    pe += section(b".text", len(text_bytes), UEFI_TEXT_RVA,
                  len(text_raw), size_of_headers, 0xE0000020)
    pe += section(b".reloc", len(reloc_payload), reloc_rva,
                  len(reloc_raw), size_of_headers + len(text_raw), 0x42000040)

    headers = bytes(dos) + bytes(pe)
    headers += b"\0" * (size_of_headers - len(headers))
    return headers + text_raw + reloc_raw


def build_uefi_stub(cc, ld, objcopy, nm, workdir):
    src = os.path.join(REPO_ROOT, "scripts", "efi_boot.c")
    obj = os.path.join(workdir, "efi_boot.o")
    elf = os.path.join(workdir, "efi_boot.elf")
    raw = os.path.join(workdir, "efi_boot.raw")
    subprocess.check_call([cc, "-O2", "-ffreestanding", "-fno-stack-protector",
                           "-fno-builtin", "-fshort-wchar", "-Wall", "-Wextra",
                           "-c", src, "-o", obj])
    subprocess.check_call([ld, "-nostdlib", "-e", "efi_main", "-Ttext=0x%X" % UEFI_TEXT_RVA,
                           obj, "-o", elf])
    out = subprocess.check_output([nm, elf]).decode()
    entry_addr = None
    max_addr = UEFI_TEXT_RVA
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == "efi_main":
            entry_addr = int(parts[0], 16)
        if len(parts) >= 3:
            try:
                a = int(parts[0], 16)
            except ValueError:
                continue
            if a > max_addr:
                max_addr = a
    if entry_addr is None:
        sys.exit("efi_main symbol not found")
    subprocess.check_call([objcopy, "-O", "binary", elf, raw])
    with open(raw, "rb") as f:
        text = f.read()
    # objcopy -O binary drops trailing NOBITS .bss. Pad the raw image so statics
    # in .bss (e.g. boot_info_storage) are mapped and writable in the PE image;
    # otherwise build_boot_info faults writing to unmapped memory past the image.
    needed = max(len(text), (max_addr - UEFI_TEXT_RVA) + 0x4000)
    if needed > len(text):
        text += b"\0" * (needed - len(text))
    return build_pe32_plus(text, entry_addr - UEFI_TEXT_RVA)


def scrub_fat_timestamps(img_path):
    """Force deterministic FAT directory-entry timestamps (mtools stamps
    create/write times from the wall clock; scrub them to fixed values)."""
    names = {b"BOOTAA64EFI", b"OS      IMG", b"EFI        ", b"ZENIN      ",
             b".          ", b"..         ", b"ZENINOS    ", b"BOOT       "}
    fixed_date = (40 << 9) | (1 << 5) | 1  # 2020-01-01
    with open(img_path, "r+b") as f:
        data = bytearray(f.read())
    for off in range(0, len(data) - 32, 32):
        if bytes(data[off:off + 11]) in names:
            data[off + 13] = 0            # CrtTimeTenth
            data[off + 14:off + 16] = b"\0\0"                    # CrtTime
            data[off + 16:off + 18] = fixed_date.to_bytes(2, "little")  # CrtDate
            data[off + 18:off + 20] = fixed_date.to_bytes(2, "little")  # LstAccDate
            data[off + 22:off + 24] = b"\0\0"                    # WrtTime
            data[off + 24:off + 26] = fixed_date.to_bytes(2, "little")  # WrtDate
    f = open(img_path, "wb")
    f.write(data)
    f.close()


def make_sparse(path):
    """Mark `path` as an NTFS sparse file and punch its zero runs.

    The logical bytes are unchanged (holes read back as zeros), so the FAT
    filesystem, QEMU, and the artifact SHA-256 are all bit-identical; only the
    on-disk allocation collapses. Returns the allocated (on-disk) size in
    bytes, or None when the platform cannot sparsify (non-Windows).
    """
    if os.name != "nt":
        return None
    import ctypes
    from ctypes import wintypes

    GENERIC_WRITE = 0x40000000
    FILE_SHARE_RW = 0x3
    OPEN_EXISTING = 3
    FILE_ATTRIBUTE_NORMAL = 0x80
    FSCTL_SET_SPARSE = 0x000900C4
    FSCTL_SET_ZERO_DATA = 0x000980C8

    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.CreateFileW.restype = wintypes.HANDLE
    h = k32.CreateFileW(path, GENERIC_WRITE, FILE_SHARE_RW, None,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, None)
    if h == ctypes.c_void_p(-1).value:
        return None

    class FZDI(ctypes.Structure):
        _fields_ = [("liFileOffset", ctypes.c_longlong),
                    ("liBeyondFinalZero", ctypes.c_longlong)]

    page = 4096
    size = os.path.getsize(path)
    npages = (size + page - 1) // page
    n = wintypes.DWORD(0)
    try:
        k32.DeviceIoControl(h, FSCTL_SET_SPARSE, None, 0, None, 0,
                            ctypes.byref(n), None)
        run = -1
        with open(path, "rb") as f:
            for p in range(npages):
                chunk = f.read(page)
                if chunk and not any(chunk):  # all-zero page
                    if run < 0:
                        run = p
                elif run >= 0:
                    z = FZDI(run * page, p * page)
                    k32.DeviceIoControl(h, FSCTL_SET_ZERO_DATA, ctypes.byref(z),
                                        ctypes.sizeof(z), None, 0,
                                        ctypes.byref(n), None)
                    run = -1
            if run >= 0:
                z = FZDI(run * page, npages * page)
                k32.DeviceIoControl(h, FSCTL_SET_ZERO_DATA, ctypes.byref(z),
                                    ctypes.sizeof(z), None, 0,
                                    ctypes.byref(n), None)
    finally:
        k32.CloseHandle(h)
    # On-disk (allocated) size for a sparse file: GetCompressedFileSizeW.
    k32.GetCompressedFileSizeW.restype = wintypes.DWORD
    hi = wintypes.DWORD(0)
    lo = k32.GetCompressedFileSizeW(path, ctypes.byref(hi))
    if lo == 0xFFFFFFFF and ctypes.get_last_error() != 0:
        return None
    return (hi.value << 32) | lo


def build_uefi_disk(boot_efi, kernel_flat, out_img):
    """Assemble a deterministic FAT16 superfloppy with the EFI boot path."""
    workdir = os.path.dirname(out_img)
    if os.name == "nt":
        wd = "/mnt/" + workdir[0].lower() + "/" + workdir[3:].replace("\\", "/")
        shell = ["wsl", "-d", "kali-linux", "--", "bash", "-c"]
    else:
        wd = workdir
        shell = ["bash", "-c"]
    size_mib = 64
    cmd = (
        "set -e; cd '{wd}' && "
        "rm -f uefi_disk.img && dd if=/dev/zero of=uefi_disk.img bs=1M count={n} status=none && "
        "mkfs.fat -F 16 -n ZENINOS -i 5A454E49 uefi_disk.img >/dev/null && "
        "touch -t 202001010000 BOOTAA64.EFI.src OS.IMG.src && "
        "mmd -i uefi_disk.img ::/EFI ::/EFI/BOOT ::/ZENIN && "
        "mcopy -i uefi_disk.img BOOTAA64.EFI.src ::/EFI/BOOT/BOOTAA64.EFI && "
        "mcopy -i uefi_disk.img OS.IMG.src ::/ZENIN/OS.IMG"
    ).format(wd=wd, n=size_mib)
    with open(os.path.join(workdir, "BOOTAA64.EFI.src"), "wb") as f:
        f.write(open(boot_efi, "rb").read())
    with open(os.path.join(workdir, "OS.IMG.src"), "wb") as f:
        f.write(open(kernel_flat, "rb").read())
    subprocess.check_call(shell + [cmd])
    os.replace(os.path.join(workdir, "uefi_disk.img"), out_img)
    scrub_fat_timestamps(out_img)
    os.remove(os.path.join(workdir, "BOOTAA64.EFI.src"))
    os.remove(os.path.join(workdir, "OS.IMG.src"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size-tool", default="aarch64-none-elf-size")
    ap.add_argument("--objdump", default="aarch64-none-elf-objdump")
    ap.add_argument("--toolchain-prefix",
                    default="C:/toolchains/arm-gnu-aarch64/bin/aarch64-none-elf-")
    args = ap.parse_args()

    elf = os.path.join(KERNEL_DIR, "zenin-kernel.elf")
    flat = os.path.join(KERNEL_DIR, "zenin-kernel.bin")
    for path in (elf, flat):
        if not os.path.exists(path):
            sys.exit("missing build artifact: %s (run: make -C kernel "
                     "CROSS_COMPILE=aarch64-none-elf-)" % path)

    os.makedirs(DIST_DIR, exist_ok=True)
    out_elf = os.path.join(DIST_DIR, "zenin-os.elf")
    out_img = os.path.join(DIST_DIR, "zenin-os.img")
    out_manifest = os.path.join(DIST_DIR, "zenin-os-image.manifest")
    with open(elf, "rb") as f:
        open(out_elf, "wb").write(f.read())
    with open(flat, "rb") as f:
        open(out_img, "wb").write(f.read())

    pfx = args.toolchain_prefix
    ext = ".exe" if os.name == "nt" else ""
    boot_efi = os.path.join(DIST_DIR, "BOOTAA64.EFI")
    with open(boot_efi, "wb") as f:
        f.write(build_uefi_stub(pfx + "gcc" + ext, pfx + "ld" + ext,
                                pfx + "objcopy" + ext, pfx + "nm" + ext, DIST_DIR))
    out_uefi = os.path.join(DIST_DIR, "zenin-os-uefi.img")
    build_uefi_disk(boot_efi, out_img, out_uefi)
    on_disk = make_sparse(out_uefi)

    component_rows = []
    component_total = [0, 0, 0]
    covered = set()
    for name, cls, objects, desc in COMPONENTS:
        t = d = b = 0
        for obj in objects:
            if obj in covered:
                sys.exit("component table error: %s listed twice" % obj)
            covered.add(obj)
            ot, od, ob = object_sizes(args.size_tool, obj)
            t += ot
            d += od
            b += ob
        component_rows.append((name, cls, objects, desc, t, d, b))
        component_total[0] += t
        component_total[1] += d
        component_total[2] += b

    # Completeness check: every object linked into the image must be claimed
    # by exactly one component class (Goal 1.7 requires all classes present).
    expected_objects = set()
    for _, _, objects, _ in COMPONENTS:
        expected_objects.update(objects)
    unclaimed = []
    for root, _, files in os.walk(KERNEL_DIR):
        for fn in files:
            if fn.endswith(".o"):
                rel = os.path.relpath(os.path.join(root, fn), KERNEL_DIR)
                rel = rel.replace("\\", "/")
                if rel not in expected_objects:
                    unclaimed.append(rel)
    if unclaimed:
        sys.exit("component table incomplete; unclaimed objects: %s"
                 % ", ".join(sorted(unclaimed)))

    elf_size = os.path.getsize(out_elf)
    img_size = os.path.getsize(out_img)
    rev = git_revision()

    lines = []
    lines.append("Project Zenin — Installable OS Image Manifest")
    lines.append("============================================")
    lines.append("")
    lines.append("image-name: zenin-os")
    lines.append("source-revision: %s" % rev)
    lines.append("toolchain: %s" % tool_version(args.size_tool).replace(" (", " | ("))
    lines.append("build: reproducible (no timestamps or absolute paths embedded)")
    lines.append("platform-contract: AArch64; QEMU virt (gic-version=2); "
                 "flat image load address 0x%X" % LOAD_ADDRESS)
    lines.append("")
    lines.append("Components (all classes required by Goal 1.7 are present in-image)")
    lines.append("----------------------------------------------------------------")
    for name, cls, objects, desc, t, d, b in component_rows:
        lines.append("component: %s" % name)
        lines.append("  class: %s" % cls)
        lines.append("  members: %s" % ", ".join(objects))
        lines.append("  footprint: text=%d data=%d bss=%d bytes" % (t, d, b))
        lines.append("  notes: %s" % desc)
    lines.append("")
    lines.append("component-footprint-totals: text=%d data=%d bss=%d bytes "
                 "(linked into the image; counted once)" % tuple(component_total))
    lines.append("")
    lines.append("Artifacts")
    lines.append("---------")
    lines.append("artifact: dist/zenin-os.elf")
    lines.append("  role: bootable ELF system image (QEMU -kernel verified boot path)")
    lines.append("  size-bytes: %d" % elf_size)
    lines.append("  sha256: %s" % sha256_file(out_elf))
    lines.append("artifact: dist/zenin-os.img")
    lines.append("  role: flat OS image (load at 0x%X; embedded in the UEFI disk)" % LOAD_ADDRESS)
    lines.append("  size-bytes: %d" % img_size)
    lines.append("  sha256: %s" % sha256_file(out_img))
    lines.append("artifact: dist/BOOTAA64.EFI")
    lines.append("  role: UEFI boot stub (PE32+ AArch64 EFI application; loads the OS image and jumps to it)")
    lines.append("  size-bytes: %d" % os.path.getsize(boot_efi))
    lines.append("  sha256: %s" % sha256_file(boot_efi))
    lines.append("artifact: dist/zenin-os-uefi.img")
    lines.append("  role: installable bootable OS disk (FAT16: EFI/BOOT/BOOTAA64.EFI + ZENIN/OS.IMG;")
    lines.append("        boots on ARM64 UEFI firmware)")
    lines.append("  size-bytes: %d" % os.path.getsize(out_uefi))
    lines.append("  sha256: %s" % sha256_file(out_uefi))
    lines.append("")
    total = os.path.getsize(out_uefi)
    lines.append("installable-image-size: %d bytes (%.2f MiB)" % (total, total / (1024 * 1024)))
    if on_disk is not None:
        lines.append("installable-image-on-disk: %d bytes (%.2f MiB, sparse)" %
                     (on_disk, on_disk / (1024 * 1024)))
    lines.append("size-requirement: <= 300 MiB -> %s" %
                 ("PASS" if total <= IMAGE_SIZE_LIMIT_BYTES else "FAIL"))
    lines.append("")
    lines.append("Scope notes")
    lines.append("-----------")
    lines.append("- The image is complete for the current OS: kernel, base system,")
    lines.append("  AI services with shipped models/weights, and bundled runtimes are")
    lines.append("  all linked into the OS image and enumerated above.")
    lines.append("- Shipped models/weights are hand-authored prototype classifier")
    lines.append("  weights; bundled runtimes are prototype format subsystems.")
    lines.append("- Verified boot paths: QEMU virt platform kernel loader and ARM64")
    lines.append("  UEFI (AAVMF) loading EFI/BOOT/BOOTAA64.EFI from the disk image.")
    lines.append("- Install path: write dist/zenin-os-uefi.img to storage (or attach")
    lines.append("  as a disk) on an ARM64 UEFI platform.")

    manifest_text = "\n".join(lines) + "\n"
    with open(out_manifest, "w", newline="\n") as f:
        f.write(manifest_text)

    print(manifest_text)
    print("manifest-sha256: %s" % sha256_file(out_manifest))
    return 0


if __name__ == "__main__":
    sys.exit(main())
