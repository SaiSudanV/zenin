/*
 * Project Zenin - Universal Windows PE32 / PE32+ Binary Loader & Thunk Engine
 * Full spec implementation for Portable Executables (PE32 & PE32+):
 *   - DOS Header & PE Signature verification
 *   - COFF File Header & Optional Header (PE32 / PE32+)
 *   - Section Table Mapping (.text, .rdata, .data, .reloc)
 *   - Import Directory Table (IDT) & Import Address Table (IAT) Resolution
 *   - Base Relocation (.reloc) Engine
 *   - Win32 Direct Thunk Bindings (KERNEL32, USER32, GDI32, NTDLL, D3D/DXGI)
 *
 * Designed to execute real Windows binaries natively (on x86_64) or via
 * high-performance DBT with zero-copy presentation onto Zenin Framebuffer.
 */

#ifndef ZENIN_PE_LOADER_H
#define ZENIN_PE_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PE_DOS_MAGIC    0x5A4D  /* 'MZ' */
#define PE_SIGNATURE    0x00004550 /* 'PE\0\0' */

/* Machine Types */
#define PE_MACHINE_I386   0x014C
#define PE_MACHINE_AMD64  0x8664
#define PE_MACHINE_ARM64  0xAA64

/* Optional Header Magic */
#define PE32_MAGIC       0x010B  /* 32-bit PE */
#define PE32_PLUS_MAGIC  0x020B  /* 64-bit PE+ */

/* Section Flags */
#define IMAGE_SCN_MEM_EXECUTE  0x20000000
#define IMAGE_SCN_MEM_READ     0x40000000
#define IMAGE_SCN_MEM_WRITE    0x80000000

/* Data Directory Indices */
#define IMAGE_DIRECTORY_ENTRY_EXPORT        0
#define IMAGE_DIRECTORY_ENTRY_IMPORT        1
#define IMAGE_DIRECTORY_ENTRY_RESOURCE      2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION     3
#define IMAGE_DIRECTORY_ENTRY_SECURITY      4
#define IMAGE_DIRECTORY_ENTRY_BASERELOC     5
#define IMAGE_DIRECTORY_ENTRY_DEBUG         6
#define IMAGE_DIRECTORY_ENTRY_IAT           12

#pragma pack(push, 1)

typedef struct {
    uint16_t e_magic;                     /* Magic number 'MZ' */
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;                    /* File address of new exe header */
} pe_dos_header_t;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} pe_data_directory_t;

typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} pe_coff_header_t;

typedef struct {
    uint16_t Magic;                       /* 0x010B (PE32) */
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    pe_data_directory_t DataDirectory[16];
} pe_optional_header32_t;

typedef struct {
    uint16_t Magic;                       /* 0x020B (PE32+) */
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    pe_data_directory_t DataDirectory[16];
} pe_optional_header64_t;

typedef struct {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} pe_section_header_t;

typedef struct {
    uint32_t CharacteristicsOrOriginalFirstThunk;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t FirstThunkRVA;
} pe_import_descriptor_t;

#pragma pack(pop)

/* Loaded Windows Process Image in Zenin Memory */
typedef struct {
    bool is_64bit;
    uint16_t machine_type;
    uintptr_t preferred_base;
    uintptr_t allocated_base;
    size_t image_size;
    uintptr_t entry_point;
    uint32_t num_sections;
    pe_section_header_t *sections;
    uint32_t imports_resolved;
    bool is_loaded;
} zenin_win32_process_t;

/* PE Subsystem APIs */
void pe_subsystem_init(void);
bool pe_load_executable(const void *file_data, size_t file_size, zenin_win32_process_t *out_proc);
uintptr_t pe_resolve_win32_import(const char *dll_name, const char *func_name);

#endif /* ZENIN_PE_LOADER_H */
