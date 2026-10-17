/*
 * Project Zenin UEFI boot stub (BOOTAA64.EFI).
 *
 * Loads \ZENIN\OS.IMG (the flat installable kernel image) from the EFI
 * system partition at its physical load address, exits boot services,
 * restores bare-metal AArch64 state (caches cleaned, MMU and caches off),
 * and jumps to the kernel entry point. This gives the installable image a
 * standard ARM64 UEFI boot path (Goal 1.7).
 */
#include <stdint.h>
#include <stddef.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef u64 efi_status;
typedef void *efi_handle;

#define EFI_SUCCESS 0
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_LOAD_ADDRESS ((u64)0x40080000ULL)
#define ALLOCATE_ADDRESS 2u
#define EFI_RESERVED_MEMORY 0u
#define EFI_OPEN_MODE_READ 0x0000000000000001ULL

typedef struct { u32 a; u16 b, c; u8 d[8]; } efi_guid;

static const efi_guid loaded_image_guid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const efi_guid sfs_guid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
static const efi_guid file_info_guid = {
    0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

typedef struct {
    u64 revision;
    efi_status (*open)(void *self, void **out, u16 *name, u64 mode, u64 attrs);
    efi_status (*close)(void *self);
    efi_status (*delete)(void *self);
    efi_status (*read)(void *self, u64 *size, void *buffer);
    efi_status (*write)(void *self, u64 *size, void *buffer);
    efi_status (*get_position)(void *self, u64 *pos);
    efi_status (*set_position)(void *self, u64 pos);
    efi_status (*get_info)(void *self, efi_guid *type, u64 *size, void *buf);
    efi_status (*set_info)(void *self, efi_guid *type, u64 size, void *buf);
    efi_status (*flush)(void *self);
} efi_file;

typedef struct {
    u64 revision;
    efi_status (*open_volume)(void *self, void **root);
} efi_sfs;

typedef struct {
    u8 pad[24];
    efi_handle device_handle;
} efi_loaded_image;

typedef struct {
    u8 hdr[24];
    u64 raise_tpl;
    u64 restore_tpl;
    efi_status (*allocate_pages)(u32 type, u32 memory_type, u64 pages, u64 *memory);
    u64 free_pages;
    efi_status (*get_memory_map)(u64 *size, void *map, u64 *key, u64 *desc_size, u32 *desc_ver);
    efi_status (*allocate_pool)(u32 type, u64 size, void **buffer);
    u64 free_pool;
    u8 pad2[152];
    efi_status (*exit_boot_services)(efi_handle image, u64 key);
} efi_boot_services;

typedef struct {
    u8 hdr[24];
    u16 *firmware_vendor;
    u32 firmware_revision;
    u32 pad;
    u64 console_in_handle;
    u64 con_in;
    u64 console_out_handle;
    u64 con_out;
    u64 standard_error_handle;
    u64 std_err;
    u64 runtime_services;
    efi_boot_services *boot;
} efi_system_table;

_Static_assert(__builtin_offsetof(efi_boot_services, allocate_pages) == 40, "bs layout");
_Static_assert(__builtin_offsetof(efi_boot_services, exit_boot_services) == 232, "bs layout");
_Static_assert(__builtin_offsetof(efi_system_table, boot) == 96, "st layout");
_Static_assert(__builtin_offsetof(efi_loaded_image, device_handle) == 24, "li layout");

static efi_status open_protocol(efi_boot_services *bs, efi_handle h, efi_guid *g, void **out) {
    typedef efi_status (*fn)(efi_handle, efi_guid *, void **);
    fn f = (fn)(*(u64 *)((u8 *)bs + 152)); /* HandleProtocol */
    return f(h, g, out);
}

/* Direct PL011 console for pre-boot diagnostics (QEMU virt UART0). */
static void dbg_char(u8 c) {
    volatile u32 *uart = (volatile u32 *)0x09000000ULL;
    while (uart[6] & 0x20u) { } /* wait while TX FIFO is full */
    uart[0] = c;
}

static void dbg(const char *s) {
    while (*s) dbg_char((u8)*s++);
}

static void dbg_hex(u64 v) {
    for (int i = 60; i >= 0; i -= 4) {
        u32 n = (u32)((v >> i) & 0xFu);
        dbg_char((u8)(n < 10 ? '0' + n : 'A' + n - 10));
    }
    dbg_char('\n');
}

/* Clean and invalidate the entire data cache by set/way, then invalidate
 * the instruction cache, so no stale dirty line can corrupt kernel memory
 * after the MMU is disabled. */
static void clean_caches_and_disable_mmu(void) {
    u64 clidr, csselr;
    __asm__ volatile("mrs %0, clidr_el1" : "=r"(clidr));
    for (u32 level = 0; level < 7; ++level) {
        u32 ctype = (u32)((clidr >> (3 * level)) & 7u);
        if (ctype < 2u) continue; /* no cache or instruction-only */
        csselr = level << 1;
        __asm__ volatile("msr csselr_el1, %0\n\tisb" : : "r"(csselr));
        u64 ccsidr;
        __asm__ volatile("mrs %0, ccsidr_el1" : "=r"(ccsidr));
        u64 line_size = (ccsidr & 7u) + 4u;
        u64 ways = ((ccsidr >> 3) & 0x3FFu) + 1u;
        u64 sets = ((ccsidr >> 13) & 0x7FFFu) + 1u;
        u32 way_shift = (ways > 1u) ? (u32)__builtin_clz((unsigned)(ways - 1u)) : 32u;
        for (u64 way = 0; way < ways; ++way) {
            for (u64 set = 0; set < sets; ++set) {
                u64 sw = ((u64)level << 1) | (way << way_shift) | (set << line_size);
                __asm__ volatile("dc cisw, %0" : : "r"(sw) : "memory");
            }
        }
    }
    __asm__ volatile("dsb sy\n\tisb\n\tic iallu\n\tdsb sy\n\tisb" : : : "memory");
}

static void jump_to_kernel(void *entry, void *boot_info) {
    dbg("ZENIN-EFI: jump_to_kernel enter\n");
    u64 cpacr;
    __asm__ volatile("mrs %0, cpacr_el1" : "=r"(cpacr));
    cpacr |= (u64)3u << 20; /* FPEN: no FP/NEON trapping */
    __asm__ volatile("msr cpacr_el1, %0\n\tisb" : : "r"(cpacr) : "memory");
    __asm__ volatile("msr daifset, #0xf" : : : "memory");
    clean_caches_and_disable_mmu(); /* cache flush; MMU disabled in the asm below */
    dbg("ZENIN-EFI: jumping to kernel\n");
    register u64 entry_reg __asm__("x8") = (u64)entry;
    register u64 boot_reg __asm__("x0") = (u64)boot_info;
    /* Disable the MMU and branch in ONE asm block. After SCTLR_EL1.M=0 the C
     * return address (LR) is a virtual address that no longer resolves, so we
     * must never return to C. entry is a physical address. */
    __asm__ volatile("mrs x9, sctlr_el1\n\t"
                     "bic x9, x9, #1\n\t"       /* M=0 */
                     "bic x9, x9, #4\n\t"       /* C=0 */
                     "bic x9, x9, #(1<<12)\n\t" /* I=0 */
                     "msr sctlr_el1, x9\n\tisb\n\t"
                     "tlbi vmalle1\n\tdsb sy\n\tisb\n\t"
                     "mov x1, #0\n\tmov x2, #0\n\tmov x3, #0\n\t"
                     "mov x4, #0\n\tmov x5, #0\n\tmov x6, #0\n\tmov x7, #0\n\t"
                     "br %1" : : "r"(boot_reg), "r"(entry_reg)
                     : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x9", "memory");
}

/* EFI memory descriptor (UEFI spec) — used to walk GetMemoryMap output. */
typedef struct {
    u32 type; u32 pad;
    u64 phys_start; u64 virt_start; u64 num_pages; u64 attr;
} efi_mem_desc;

/* Boot handoff descriptor — must match kernel/include/boot_info.h. */
typedef struct { u64 base; u64 pages; } zenin_boot_range_t;
typedef struct {
    u64 magic; u64 kernel_start; u64 kernel_end; u64 ram_count; u64 mmio_count;
} zenin_boot_info_t;
#define ZENIN_BOOT_INFO_MAGIC 0x5A454E49424F4F54ULL
#define ZENIN_BOOT_INFO_MAX_REGIONS 64
/* Layout: [header][ram[0..MAX)][mmio[0..MAX)] (fixed split, matches boot_info.h). */
#define ZENIN_BOOT_INFO_BYTES (sizeof(zenin_boot_info_t) + \
                               2u * ZENIN_BOOT_INFO_MAX_REGIONS * sizeof(zenin_boot_range_t))

/* Build the boot-info from the firmware memory map: collect usable RAM regions
 * (conventional + loader/boot-services, reclaimable after ExitBootServices) as
 * RAM, and memory-mapped I/O / reserved ranges as device (MMIO) windows the
 * identity map must map as device memory. Written into a caller-provided
 * buffer (never the stub image — UEFI maps code sections R+X and faults on
 * writes regardless of PE characteristics). */
static void *build_boot_info(void *out, void *map, u64 map_size, u64 desc_size,
                             u64 kernel_start, u64 kernel_end) {
    dbg("ZENIN-EFI: build_boot_info\n");
    zenin_boot_info_t *bi = (zenin_boot_info_t *)out;
    zenin_boot_range_t *regions = (zenin_boot_range_t *)(bi + 1);
    zenin_boot_range_t *mmio = regions + ZENIN_BOOT_INFO_MAX_REGIONS;
    bi->magic = ZENIN_BOOT_INFO_MAGIC;
    bi->kernel_start = kernel_start;
    bi->kernel_end = kernel_end;
    bi->ram_count = 0;
    bi->mmio_count = 0;
    dbg("ZENIN-EFI: bss-init-ok\n");
    u64 off = 0;
    if (desc_size < sizeof(efi_mem_desc) || desc_size > 256u) desc_size = sizeof(efi_mem_desc);
    u64 iters = 0;
    while (off + desc_size <= map_size && iters < 512u) {
        efi_mem_desc *d = (efi_mem_desc *)((u8 *)map + off);
        if (iters == 0u) { dbg("ZENIN-EFI: d0="); dbg_hex(d->type); }
        if (d->type == 7 /* EfiConventionalMemory */ ||
            (d->type >= 1 && d->type <= 4) /* Loader/BootServices */) {
            if (bi->ram_count < ZENIN_BOOT_INFO_MAX_REGIONS) {
                regions[bi->ram_count].base = d->phys_start;
                regions[bi->ram_count].pages = d->num_pages;
                bi->ram_count++;
            }
        } else if (d->type == 11 /* EfiMemoryMappedIO */ ||
                   d->type == 0 /* EfiReservedMemoryType */) {
            if (bi->mmio_count < ZENIN_BOOT_INFO_MAX_REGIONS) {
                mmio[bi->mmio_count].base = d->phys_start;
                mmio[bi->mmio_count].pages = d->num_pages;
                bi->mmio_count++;
            }
        }
        off += desc_size;
        iters++;
    }
    return out;
}

efi_status efi_main(efi_handle image, efi_system_table *st) {
    *(volatile u32 *)0x09000000ULL = 0x53u; /* raw 'S': entry reached */
    efi_boot_services *bs = st->boot;
    dbg("ZENIN-EFI: stub started\n");

    efi_loaded_image *li = NULL;
    efi_status s = open_protocol(bs, image, (efi_guid *)&loaded_image_guid, (void **)&li);
    if (s != EFI_SUCCESS || !li) { dbg("ZENIN-EFI: loaded-image err "); dbg_hex(s); return s; }

    efi_sfs *sfs = NULL;
    s = open_protocol(bs, li->device_handle, (efi_guid *)&sfs_guid, (void **)&sfs);
    if (s != EFI_SUCCESS || !sfs) { dbg("ZENIN-EFI: sfs err "); dbg_hex(s); return s; }

    void *root = NULL;
    s = sfs->open_volume(sfs, &root);
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: open-volume err "); dbg_hex(s); return s; }

    static u16 kernel_path[16] = {
        '\\', 'Z', 'E', 'N', 'I', 'N', '\\', 'O', 'S', '.', 'I', 'M', 'G', 0
    };
    efi_file *file = NULL;
    s = ((efi_file *)root)->open(root, (void **)&file, kernel_path, EFI_OPEN_MODE_READ, 0);
    if (s != EFI_SUCCESS || !file) { dbg("ZENIN-EFI: open-file err "); dbg_hex(s); return s; }
    dbg("ZENIN-EFI: OS.IMG opened\n");

    /* Query the image size via GetInfo (EFI_FILE_INFO_ID: FileSize at +8). */
    u8 info[512];
    u64 info_size = sizeof(info);
    s = file->get_info(file, (efi_guid *)&file_info_guid, &info_size, info);
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: get-info err "); dbg_hex(s); return s; }
    u64 image_size = *(u64 *)(void *)(info + 8);
    dbg("ZENIN-EFI: OS.IMG size ");
    dbg_hex(image_size);
    if (image_size == 0 || image_size > (u64)256u * 1024u * 1024u) return EFI_LOAD_ADDRESS;
    u64 pages = (image_size + 4095u) / 4096u;

    u64 dest = EFI_LOAD_ADDRESS;
    s = bs->allocate_pages(ALLOCATE_ADDRESS, EFI_RESERVED_MEMORY, pages, &dest);
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: alloc-pages err "); dbg_hex(s); return s; }
    s = file->read(file, &image_size, (void *)EFI_LOAD_ADDRESS);
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: read err "); dbg_hex(s); return s; }
    file->close(file);
    dbg("ZENIN-EFI: OS.IMG loaded; applying boot-env memory-clear contract\n");

    /* D012 contract: the boot environment delivers RAM cleared to zero.
     * The image tail carries a .imgmeta footer: {magic, footprint,
     * kernel_start}; zero the resident footprint beyond the loaded image. */
    if (image_size >= 24) {
        u64 *meta = (u64 *)(EFI_LOAD_ADDRESS + image_size - 24);
        if (meta[0] == 0x5A454E494D455441ULL && meta[1] >= image_size) {
            volatile u64 *p = (volatile u64 *)(EFI_LOAD_ADDRESS + image_size);
            volatile u64 *end = (volatile u64 *)(EFI_LOAD_ADDRESS + meta[1]);
            while (p < end) { *p++ = 0; }
        }
    }
    dbg("ZENIN-EFI: exiting boot services\n");

    /* Exit boot services with a fresh memory map (retry on key change). */
    u64 map_size = 0, map_key = 0, desc_size = 0;
    u32 desc_ver = 0;
    void *map = NULL;
    bs->get_memory_map(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    map_size += 8u * desc_size + 4096u;
    s = bs->allocate_pool(1u /* EfiLoaderData */, map_size, &map);
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: alloc-pool err "); dbg_hex(s); return s; }
    u64 actual_map_size = 0;
    /* Boot-info lives on the stub's stack, not in the image (UEFI maps code
     * sections R+X and faults on writes regardless of PE flags). The kernel
     * consumes it in arch_mmu_enable + kmain before pmm_init, and both run on
     * the kernel's own stack — this frame is abandoned but not touched. */
    u8 boot_info_buf[ZENIN_BOOT_INFO_BYTES];
    void *boot_info = NULL;
    for (int attempt = 0; attempt < 8; ++attempt) {
        u64 ms = map_size;
        s = bs->get_memory_map(&ms, map, &map_key, &desc_size, &desc_ver);
        if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: memmap err "); dbg_hex(s); return s; }
        actual_map_size = ms;
        /* Build boot-info BEFORE ExitBootServices. The map buffer and the C
         * stack live in boot-services/loader memory that is reclaimed at exit,
         * so walking them afterwards reads reclaimed garbage and runs away. */
        boot_info = build_boot_info(boot_info_buf, map, actual_map_size, desc_size,
                                    EFI_LOAD_ADDRESS, EFI_LOAD_ADDRESS + image_size);
        s = bs->exit_boot_services(image, map_key);
        if (s == EFI_SUCCESS) break;
    }
    if (s != EFI_SUCCESS) { dbg("ZENIN-EFI: exit-boot-services err "); dbg_hex(s); return s; }
    dbg("ZENIN-EFI: boot services exited\n");
    jump_to_kernel((void *)EFI_LOAD_ADDRESS, boot_info);
    return EFI_SUCCESS;
}
