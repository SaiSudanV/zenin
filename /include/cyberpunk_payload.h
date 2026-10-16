/*
 * Project Zenin - Ingested Real Test Binary: Super Cyberpunk 2077 (FAN PROTOTYPE)
 * Real PE32+ Header, Section Table, and Import Directory Table (IDT)
 * Extracted directly from official itch.io binary 'Cyberpunk 2077 2D.exe'
 */

#ifndef ZENIN_CYBERPUNK_PAYLOAD_H
#define ZENIN_CYBERPUNK_PAYLOAD_H

#include <stdint.h>
#include <stddef.h>

/* The first 1024 bytes of the real executable containing:
 * - DOS Header (MZ, 0x4D5A, e_lfanew = 0x00000080)
 * - PE Signature (PE\0\0, 0x00004550)
 * - COFF Header (Machine: AMD64 0x8664, 6 Sections)
 * - Optional Header PE32+ (Magic: 0x020B, EntryPoint RVA: 0x1260, ImageBase: 0x140000000)
 * - 6 Section Headers (.text, .rdata, .data, .pdata, .rsrc, .reloc)
 */
extern const uint8_t cyberpunk_real_pe_header[1024];
extern const size_t cyberpunk_real_pe_header_size;

#endif /* ZENIN_CYBERPUNK_PAYLOAD_H */
