#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../kernel/include/boot_info.h"
#include "../kernel/include/memory_accounting.h"

typedef struct {
    zenin_boot_info_t header;
    zenin_boot_range_t ram[ZENIN_BOOT_INFO_MAX_REGIONS];
    zenin_boot_range_t mmio[ZENIN_BOOT_INFO_MAX_REGIONS];
} test_boot_info_t;

static void init_valid_info(test_boot_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->header.magic = ZENIN_BOOT_INFO_MAGIC;
    info->header.kernel_start = 0x40080000ULL;
    info->header.kernel_end = 0x40081000ULL;
    info->header.ram_count = 1;
    info->ram[0] = (zenin_boot_range_t){0x40000000ULL, 256};
    info->header.mmio_count = 1;
    info->mmio[0] = (zenin_boot_range_t){0x08000000ULL, 16};
}

int main(void) {
    uint64_t end = 0, bytes = 0, total = 0, reserved = 0;
    zenin_boot_range_t range = {0x1000, 1};
    assert(zenin_boot_range_end(&range, &end) && end == 0x2000);
    range.pages = 0;
    assert(!zenin_boot_range_end(&range, &end));                 /* zero length */
    range = (zenin_boot_range_t){0x1001, 1};
    assert(!zenin_boot_range_end(&range, &end));                 /* unaligned */
    range = (zenin_boot_range_t){0, UINT64_MAX / 4096 + 1};
    assert(!zenin_boot_range_end(&range, &end));                 /* page shift overflow */
    range = (zenin_boot_range_t){UINT64_MAX - 4095, 1};
    assert(!zenin_boot_range_end(&range, &end));                 /* end overflow */
    range = (zenin_boot_range_t){0, UINT64_MAX / 4096};
    assert(zenin_boot_range_end(&range, &end));                  /* largest representable */

    test_boot_info_t info;
    init_valid_info(&info);
    assert(zenin_boot_info_valid(&info.header));
    info.ram[0].pages = 0;
    assert(zenin_boot_info_error(&info.header) == ZENIN_BOOT_INFO_BAD_RAM_RANGE);
    assert(!zenin_boot_info_valid(&info.header));
    init_valid_info(&info);
    info.header.ram_count = ZENIN_BOOT_INFO_MAX_REGIONS + 1;
    assert(zenin_boot_info_error(&info.header) == ZENIN_BOOT_INFO_BAD_COUNTS);
    assert(!zenin_boot_info_valid(&info.header));                 /* count bound */
    init_valid_info(&info);
    info.header.ram_count = 2;
    info.ram[1] = (zenin_boot_range_t){0x40080000ULL, 2};
    assert(zenin_boot_info_error(&info.header) == ZENIN_BOOT_INFO_OVERLAPPING_RAM);
    assert(!zenin_boot_info_valid(&info.header));                 /* overlap */
    info.ram[1] = (zenin_boot_range_t){0x40100000ULL, 2};
    assert(zenin_boot_info_valid(&info.header));                  /* disjoint */
    info.ram[1] = (zenin_boot_range_t){0x40100000ULL, 0};
    assert(!zenin_boot_info_valid(&info.header));                 /* zero range */
    init_valid_info(&info);
    info.mmio[0].base = 0x40000000ULL;
    assert(zenin_boot_info_error(&info.header) == ZENIN_BOOT_INFO_RAM_MMIO_OVERLAP);
    assert(!zenin_boot_info_valid(&info.header));                 /* RAM/MMIO overlap */
    init_valid_info(&info);
    info.ram[0] = (zenin_boot_range_t){ZENIN_BOOT_ADDRESS_LIMIT - 4096, 1};
    assert(zenin_boot_info_valid(&info.header));                  /* address limit */
    info.ram[0].pages = 2;
    assert(!zenin_boot_info_valid(&info.header));                 /* beyond VA limit */

    assert(zenin_pages_to_bytes(UINT64_MAX / 4096, &bytes));
    assert(bytes == (UINT64_MAX / 4096) * 4096);
    assert(!zenin_pages_to_bytes(UINT64_MAX / 4096 + 1, &bytes));
    assert(zenin_os_owned_resident_bytes(0x1000, 0x3000, 2, &total));
    assert(total == 0x4000);                                      /* image + pages */
    assert(zenin_idle_ram_within_budget(total, 5120ULL * 1024));
    assert(!zenin_idle_ram_within_budget(5120ULL * 1024,
                                         5120ULL * 1024));       /* strict bound */
    assert(!zenin_idle_ram_within_budget(5120ULL * 1024 + 1,
                                         5120ULL * 1024));
    assert(!zenin_os_owned_resident_bytes(0x3000, 0x1000, 2, &total));
    assert(!zenin_os_owned_resident_bytes(0, UINT64_MAX, 1, &total)); /* sum overflow */
    assert(zenin_platform_reserved_before_kernel(0x40080000, 0x40000000,
                                                  &reserved));
    assert(reserved == 0x80000);
    assert(!zenin_platform_reserved_before_kernel(0x40080000, 0x400A0000,
                                                  &reserved)); /* prior wrap case */

    puts("memory-accounting tests: PASS (range bounds, overlap, zero, overflow, strict threshold inputs)");
    return 0;
}
