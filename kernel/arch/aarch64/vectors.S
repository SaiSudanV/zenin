/* AArch64 exception vectors (D011 baseline 4): the IRQ slot is a naked
 * branch straight to the handler, which saves strictly the registers it
 * clobbers (x0-x3) — no generic full-context stub. */
.section ".vectors", "ax"
.balign 2048
.global __vectors_start
__vectors_start:
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b zenin_irq_entry
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128
    b vector_default
    .balign 128

/* Minimal IRQ handler: clobbers exactly x0-x3 and exits via eret (the
 * naked branch leaves x30 untouched). Body: read IAR, record id, mask the
 * virtual timer, count the wake, priority-drop/deactivate via EOIR. */
.global zenin_irq_entry
zenin_irq_entry:
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    ldr x0, =0x0801000C           /* GICC_IAR */
    ldr w1, [x0]                  /* keep full IAR for EOIR */
    and w2, w1, #0x3FF            /* interrupt id */
    ldr x3, =zenin_last_irq_id
    str w2, [x3]
    msr cntv_ctl_el0, xzr         /* acknowledge the virtual timer one-shot */
    ldr x3, =zenin_timer_irq_seen
    ldr w2, [x3]
    add w2, w2, #1
    str w2, [x3]
    and w3, w1, #0x3FF
    mov w2, #1020
    cmp w3, w2
    b.hs 1f
    ldr x3, =0x08010010           /* GICC_EOIR */
    str w1, [x3]
1:
    dsb ish
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
    eret

vector_default:
    mrs x0, esr_el1
    mrs x1, far_el1
    mrs x2, elr_el1
    bl zenin_panic
    wfi
    b vector_default
    .ltorg
