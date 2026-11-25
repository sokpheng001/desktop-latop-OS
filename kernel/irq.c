/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Interrupt handler table and dispatch through the HAL interrupt controller.
 */

#include "hal.h"
#include "irq.h"
#include "lib/kprintf.h"

/* Matches the GICv2 architectural limit (IDs 0-1019); enough for every board so far. */
#define IRQ_MAX 1020u

struct irq_entry {
	irq_handler_fn handler;
	void *ctx;
};

static struct irq_entry irq_table[IRQ_MAX];
static u64 unhandled_count;

bool irq_register(u32 irq, irq_handler_fn handler, void *ctx)
{
	if (irq >= IRQ_MAX)
		return false;
	irq_table[irq].handler = handler;
	irq_table[irq].ctx = ctx;
	hal_irq_enable(irq);
	return true;
}

void irq_dispatch(void)
{
	u32 token;
	int irq;

	/* Drain everything pending so one exception entry can serve several interrupts. */
	while ((irq = hal_irq_claim(&token)) >= 0) {
		struct irq_entry *e = (u32)irq < IRQ_MAX ? &irq_table[irq] : NULL;

		if (e && e->handler) {
			e->handler((u32)irq, e->ctx);
		} else {
			/* Disable it so an unexpected level-triggered source cannot storm the CPU.
			 */
			kprintf("IRQ %d has no handler; disabling it\n", irq);
			hal_irq_disable((u32)irq);
			unhandled_count++;
		}
		hal_irq_complete(token);
	}
}

u64 irq_unhandled_count(void)
{
	return unhandled_count;
}
