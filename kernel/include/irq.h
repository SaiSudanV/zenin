#ifndef ZENIN_IRQ_H
#define ZENIN_IRQ_H

#include <stdbool.h>

/* QEMU virt / GICv2 one-shot virtual-timer wake experiment. */
bool zenin_irq_init(void);
bool zenin_irq_test_wfi(void);
void zenin_irq_handler(void);

#endif
