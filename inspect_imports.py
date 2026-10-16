import struct

exe_path = r"c:\Custom Rom\SuperCyberpunk\Mac & PC\PC\Cyberpunk 2077 2D\Cyberpunk 2077 2D.exe"

with open(exe_path, "rb") as f:
    f.seek(0x3c)
    lfanew = struct.unpack("<I", f.read(4))[0]
    
    f.seek(lfanew + 4 + 20)
    opt_hdr = f.read(240)
    opt_magic = struct.unpack("<H", opt_hdr[:2])[0]
    
    # In PE32+, DataDirectory starts at offset 112 (after 16 uint32/uint64 fields)
    dd_offset = 112
    import_rva, import_size = struct.unpack("<II", opt_hdr[dd_offset + 8 : dd_offset + 16])
    print(f"Import Directory RVA: 0x{import_rva:08X} | Size: 0x{import_size:08X}")
    
    # Read sections to map RVA to file offset
    f.seek(lfanew + 4 + 20 + 240) # Section headers follow optional header
    sections = []
    for _ in range(6):
        sec = f.read(40)
        name = sec[:8].decode('latin1').rstrip('\x00')
        vsize, vaddr, raw_size, raw_ptr = struct.unpack("<IIII", sec[8:24])
        sections.append((name, vaddr, vsize, raw_ptr))
        
    def rva_to_offset(rva):
        for name, vaddr, vsize, raw_ptr in sections:
            if vaddr <= rva < vaddr + vsize:
                return raw_ptr + (rva - vaddr)
        return None

    import_offset = rva_to_offset(import_rva)
    print(f"Import Directory File Offset: 0x{import_offset:08X}")
    
    f.seek(import_offset)
    while True:
        desc = f.read(20)
        orig_first_thunk, timestamp, forwarder, name_rva, first_thunk = struct.unpack("<IIIII", desc)
        if name_rva == 0:
            break
        curr_pos = f.tell()
        f.seek(rva_to_offset(name_rva))
        dll_name = b""
        while True:
            b = f.read(1)
            if b == b"\x00": break
            dll_name += b
        print(f"\nImported DLL: {dll_name.decode('latin1')}")
        
        # Read thunks
        thunk_rva = orig_first_thunk if orig_first_thunk != 0 else first_thunk
        thunk_off = rva_to_offset(thunk_rva)
        f.seek(thunk_off)
        count = 0
        while True:
            val = struct.unpack("<Q", f.read(8))[0]
            if val == 0: break
            if val & (1 << 63) == 0: # Import by name
                fn_off = rva_to_offset(val & 0xFFFFFFFF)
                pos = f.tell()
                f.seek(fn_off + 2) # Skip hint
                fn_name = b""
                while True:
                    b = f.read(1)
                    if b == b"\x00": break
                    fn_name += b
                f.seek(pos)
                if count < 10:
                    print(f"  -> {fn_name.decode('latin1')}")
                count += 1
        if count > 10:
            print(f"  ... and {count - 10} more functions")
        f.seek(curr_pos)
