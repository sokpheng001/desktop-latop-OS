/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel interrupt handling: drivers register a handler per interrupt number;
 * the architecture code calls irq_dispatch() when an IRQ arrives.
 */

#ifndef OS001_IRQ_H
#define OS001_IRQ_H

#include "types.h"

#ifndef __ASSEMBLY__

typedef void (*irq_handler_fn)(u32 irq, void *ctx);

/* Install handler for irq and enable it at the interrupt controller. Returns false if irq is out of
 * range. */
bool irq_register(u32 irq, irq_handler_fn handler, void *ctx);

/* Handle every pending interrupt. Called by the architecture's IRQ exception path. */
void irq_dispatch(void);

/* Interrupts that arrived with no handler (each is then disabled). */
u64 irq_unhandled_count(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_IRQ_H */
