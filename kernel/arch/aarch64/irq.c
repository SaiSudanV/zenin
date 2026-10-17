/* One-shot architected virtual timer IRQ for QEMU virt's GICv2. */
#include "../../include/irq.h"
#include "../../include/uart.h"
#include "../../include/arch/aarch64/cpu.h"

#define GICD_BASE       0x08000000UL
#define GICC_BASE       0x08010000UL
#define GICD_CTLR       (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_IGROUPR0   (*(volatile uint32_t *)(GICD_BASE + 0x080))
#define GICD_ISENABLER0 (*(volatile uint32_t *)(GICD_BASE + 0x100))
#define GICD_ISPENDR0   (*(volatile uint32_t *)(GICD_BASE + 0x200))
#define GICD_IPRIORITYR ((volatile uint8_t *)(GICD_BASE + 0x400))
#define GICC_CTLR       (*(volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR        (*(volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR        (*(volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_HPPIR      (*(volatile uint32_t *)(GICC_BASE + 0x018))
#define GICC_EOIR       (*(volatile uint32_t *)(GICC_BASE + 0x010))

/* GICv3 (Ampere Altra, Graviton, QEMU gic-version=3): the CPU interface is a
 * system register block (ICC_*_EL1) and per-core state lives in the GICR
 * redistributor. GICD_PIDR2[7:4] reports the architecture version. */
#define GICD_PIDR2      (*(volatile uint32_t *)(GICD_BASE + 0xFFE8))
#define GICR_BASE       0x080A0000UL   /* first redistributor (core 0) */
#define GICR_IGROUPR0   (*(volatile uint32_t *)(GICR_BASE + 0x0080))
#define GICR_ISENABLER0 (*(volatile uint32_t *)(GICR_BASE + 0x0100))
#define GICR_IPRIORITYR ((volatile uint8_t *)(GICR_BASE + 0x0400))
#define VIRTUAL_TIMER_PPI 27

/* GIC architecture version for the target (see zenin_irq_init): 2 for QEMU
 * virt GICv2, 3 for Ampere/Graviton GICv3. Override with -DZENIN_GIC_VERSION=3. */
#ifndef ZENIN_GIC_VERSION
#define ZENIN_GIC_VERSION 2
#endif

extern char __vectors_start;
/* Consumed by the naked asm IRQ handler (vectors.S, D011 baseline 4). */
volatile uint32_t zenin_timer_irq_seen;
volatile uint32_t zenin_last_irq_id = 1023;
static uint64_t timer_frequency;

static inline void write_cntv_ctl(uint64_t value) {
    __asm__ volatile("msr cntv_ctl_el0, %0" : : "r"(value) : "memory");
}

static inline void write_cntv_cval(uint64_t value) {
    __asm__ volatile("msr cntv_cval_el0, %0" : : "r"(value) : "memory");
}

static inline uint64_t read_cntv_ctl(void) {
    uint64_t value;
    __asm__ volatile("mrs %0, cntv_ctl_el0" : "=r"(value));
    return value;
}

bool zenin_irq_init(void) {
    uint64_t vector_base = (uintptr_t)&__vectors_start;
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 3) == 2) {
        __asm__ volatile("msr vbar_el2, %0\nisb" : : "r"(vector_base) : "memory");
    } else {
        __asm__ volatile("msr vbar_el1, %0\nisb" : : "r"(vector_base) : "memory");
    }
    __asm__ volatile("dsb ish\nisb" : : : "memory");

    timer_frequency = arch_get_counter_frequency();
    if (timer_frequency < 20) return false;
    uart_puts("[zenin-irq] CurrentEL=");
    uart_put_dec((uint32_t)((current_el >> 2) & 3));
    uart_puts("; architectural timer frequency=");
    uart_put_dec((uint32_t)timer_frequency);
    uart_puts(" Hz\n");

    /* Detect the GIC CPU-interface version from ID_AA64PFR0_EL1[27:24] (the GIC
     * field): 0=none, 1=GICv3/v4 sysregs, 2=GICv2, 3=both. This is a system
     * register read (always available at EL1, no MMIO), so it is fault-safe on
     * QEMU virt — unlike a GICD_PIDR2 MMIO probe, which faults on QEMU virt's
     * GICv2. Prefer the GICv3 system-register interface when present. */
    uint64_t pfr0_el1;
    __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(pfr0_el1));
    uint32_t gic_field = (uint32_t)((pfr0_el1 >> 24) & 0xF);
    uint32_t gic_ver = (gic_field & 0x1) ? 3 : 2;
    uart_puts("[zenin-irq] ID_AA64PFR0.GIC=");
    uart_put_dec(gic_field);
    uart_puts("; using GICv");
    uart_put_dec(gic_ver);
    uart_puts("\n");
    if (gic_ver == 3) {
        /* GICv3: system-register CPU interface + GICR redistributor. */
        GICD_CTLR = 0;
        __asm__ volatile("msr ICC_SRE_EL1, %0\nisb" : : "r"(1ULL) : "memory");
        GICR_IGROUPR0 |= (1U << VIRTUAL_TIMER_PPI);
        GICR_IPRIORITYR[VIRTUAL_TIMER_PPI] = 0x80;
        GICR_ISENABLER0 = (1U << VIRTUAL_TIMER_PPI);
        __asm__ volatile("msr ICC_PMR_EL1, %0" : : "r"(0xFFULL));
        __asm__ volatile("msr ICC_IGRPEN1_EL1, %0\nisb" : : "r"(1ULL) : "memory");
        GICD_CTLR = 0x3; /* EnableGrp1 | ARE */
    } else {
        /* GICv2 (QEMU virt gic-version=2). PPI 27 as Group 1 at EL1. */
        GICD_CTLR = 0;
        GICD_IGROUPR0 |= (1U << VIRTUAL_TIMER_PPI);
        GICD_IPRIORITYR[VIRTUAL_TIMER_PPI] = 0x80;
        GICD_ISENABLER0 = (1U << VIRTUAL_TIMER_PPI);
        GICC_PMR = 0xFF;
        GICC_CTLR = (1U << 1) | (1U << 2);
        GICD_CTLR = 2;
    }
    arch_cpu_dsb();
    arch_cpu_isb();
    uart_puts("[zenin-irq] GICv");
    uart_put_dec(gic_ver);
    uart_puts(" programmed; PPI-enable=");
    uart_put_hex(gic_ver >= 3 ? GICR_ISENABLER0 : GICD_ISENABLER0);
    uart_puts("\n");
    return true;
}

bool zenin_irq_test_wfi(void) {
    if (timer_frequency < 20) return false;
    zenin_timer_irq_seen = 0;
    zenin_last_irq_id = 1023;
    write_cntv_ctl(0);
    uint64_t start = arch_get_cycles();
    write_cntv_cval(start + timer_frequency / 20); /* 50 ms */
    write_cntv_ctl(1);
    arch_cpu_dsb();

    /* Bound the timer-status diagnostic before entering WFI, so a broken
     * timer cannot strand the emulator forever. */
    uint64_t diagnostic_deadline = start + timer_frequency / 5;
    while ((read_cntv_ctl() & 4) == 0 && arch_get_cycles() < diagnostic_deadline) { }
    if ((read_cntv_ctl() & 4) == 0) {
        write_cntv_ctl(0);
        uart_puts("[zenin-irq] Timer did not reach ISTATUS before deadline.\n");
        return false;
    }

    uart_puts("[zenin-irq] Timer expired; GIC pending=");
    uart_put_hex(GICD_ISPENDR0);
    uart_puts(" HPPIR=");
    uart_put_hex(GICC_HPPIR);
    uart_puts("\n");

    __asm__ volatile("msr daifclr, #2" : : : "memory");

    while (zenin_timer_irq_seen == 0) {
        arch_cpu_wfi();
    }

    __asm__ volatile("msr daifset, #2" : : : "memory");
    write_cntv_ctl(0);
    uart_puts("[zenin-irq] Timer wake interrupt ID=");
    uart_put_dec(zenin_last_irq_id);
    uart_puts(" count=");
    uart_put_dec(zenin_timer_irq_seen);
    uart_puts("\n");
    return zenin_last_irq_id == VIRTUAL_TIMER_PPI;
}

/* Called from vectors.S vector_default on any non-IRQ exception. Prints the
 * syndrome, fault address, and link address, then hangs — used to root-cause
 * silent data aborts during the UEFI boot path. */
void zenin_panic(uint64_t esr, uint64_t far, uint64_t elr) {
    uart_puts("[EXC] esr=");
    uart_put_hex(esr);
    uart_puts(" far=");
    uart_put_hex(far);
    uart_puts(" elr=");
    uart_put_hex(elr);
    uart_puts("\n");
    for (;;) __asm__ volatile("wfi");
}
