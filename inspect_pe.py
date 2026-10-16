import struct

exe_path = r"c:\Custom Rom\SuperCyberpunk\Mac & PC\PC\Cyberpunk 2077 2D\Cyberpunk 2077 2D.exe"

with open(exe_path, "rb") as f:
    dos_data = f.read(64)
    dos_magic = dos_data[:2]
    lfanew = struct.unpack("<I", dos_data[0x3c:0x40])[0]
    
    f.seek(lfanew)
    pe_sig = f.read(4)
    coff_data = f.read(20)
    machine, num_sections, timedatestamp, sym_ptr, num_syms, opt_hdr_size, characteristics = struct.unpack("<HHIIIHH", coff_data)
    
    opt_data = f.read(opt_hdr_size)
    opt_magic = struct.unpack("<H", opt_data[:2])[0]
    
    is_64 = (opt_magic == 0x20B)
    if is_64:
        entry_rva = struct.unpack("<I", opt_data[16:20])[0]
        image_base = struct.unpack("<Q", opt_data[24:32])[0]
    else:
        entry_rva = struct.unpack("<I", opt_data[16:20])[0]
        image_base = struct.unpack("<I", opt_data[28:32])[0]
        
    print("=== Super Cyberpunk 2077 PE Header Verification ===")
    print(f"DOS Magic:        {dos_magic.decode('latin1', errors='ignore')} (0x{dos_data[0]:02X}{dos_data[1]:02X})")
    print(f"PE Signature:     {pe_sig.decode('latin1', errors='ignore')}")
    print(f"Machine Type:     0x{machine:04X} {'(x86_64 / AMD64)' if machine == 0x8664 else '(i386)'}")
    print(f"Number Sections:  {num_sections}")
    print(f"Optional Magic:   0x{opt_magic:04X} {'(PE32+ 64-bit)' if is_64 else '(PE32 32-bit)'}")
    print(f"Entry Point RVA:  0x{entry_rva:08X}")
    print(f"Image Base:       0x{image_base:016X}")
    
    print("\n--- Sections ---")
    for i in range(num_sections):
        sec_data = f.read(40)
        sec_name = sec_data[:8].decode('latin1').rstrip('\x00')
        vsize, vaddr, raw_size, raw_ptr = struct.unpack("<IIII", sec_data[8:24])
        flags = struct.unpack("<I", sec_data[36:40])[0]
        print(f"  [{sec_name:<8}] RVA: 0x{vaddr:08X} | VirtSize: 0x{vsize:08X} ({vsize//1024} KB) | RawSize: 0x{raw_size:08X} | Flags: 0x{flags:08X}")
