/*
 * Project Zenin - Authentic Subway Surfers Native ARM64 Game Payload
 * Extracted directly from official APK (libmain.so).
 * File Size: 7064 bytes | Entry Point: 0x00000000
 */

#ifndef ZENIN_SUBWAY_PAYLOAD_H
#define ZENIN_SUBWAY_PAYLOAD_H

#include <stdint.h>
#include <stddef.h>

#define SUBWAY_REAL_ELF_SIZE 7064
#define SUBWAY_REAL_ENTRY_POINT 0x00000000ULL
#define SUBWAY_IS_64BIT 1

extern const uint8_t subway_real_elf_header[];
extern const size_t subway_real_elf_header_size;

#endif /* ZENIN_SUBWAY_PAYLOAD_H */
