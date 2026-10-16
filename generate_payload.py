import os

exe_path = r"c:\Custom Rom\SuperCyberpunk\Mac & PC\PC\Cyberpunk 2077 2D\Cyberpunk 2077 2D.exe"
out_c_path = r"c:\Custom Rom\kernel\sys\cyberpunk_payload.c"

with open(exe_path, "rb") as f:
    header_bytes = f.read(1024)

with open(out_c_path, "w") as out:
    out.write('/* Auto-generated payload from real Cyberpunk 2077 2D.exe */\n')
    out.write('#include "../include/cyberpunk_payload.h"\n\n')
    out.write('const uint8_t cyberpunk_real_pe_header[1024] = {\n')
    for i in range(0, len(header_bytes), 16):
        chunk = header_bytes[i:i+16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        out.write(f"    {hex_str},\n")
    out.write('};\n\n')
    out.write(f'const size_t cyberpunk_real_pe_header_size = {len(header_bytes)};\n')

print(f"Generated {out_c_path} successfully ({len(header_bytes)} bytes).")
