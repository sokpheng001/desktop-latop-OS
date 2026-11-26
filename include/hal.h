/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Hardware abstraction layer. kernel/ talks to hardware only through these
 * functions; each board implements them in board/<board>/.
 */

#ifndef OS001_HAL_H
#define OS001_HAL_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Bring up the board's early devices (console first). Called once, from kernel_main. */
void hal_init(void);

/* Write one raw byte to the console, waiting while the transmitter is busy. */
void hal_console_putc(char c);

/* Read one byte from the console without waiting. Returns -1 if none is available. */
int hal_console_getc(void);

/* Interrupt controller. Interrupt numbers are the controller's IDs (GIC: SGI 0-15, PPI 16-31, SPI
 * 32+). */
void hal_irq_enable(u32 irq);
void hal_irq_disable(u32 irq);
/*
 * Claim the highest-priority pending interrupt. Returns its number and stores a token
 * for hal_irq_complete, or returns -1 if nothing is pending.
 */
int hal_irq_claim(u32 *token);
void hal_irq_complete(u32 token);
/* Make irq pending from software: an SGI is sent to this core, any other ID is set pending. */
void hal_irq_raise(u32 irq);
/* Number of interrupt IDs the controller supports. */
u32 hal_irq_count(void);

/* Periodic timer and free-running counter. */
u32 hal_timer_irq(void);      /* interrupt number of the tick timer */
void hal_timer_start(u32 hz); /* start periodic interrupts at hz */
void hal_timer_ack(void);     /* from the timer interrupt: clear it and schedule the next */
u64 hal_counter(void);	      /* monotonic counter value */
u64 hal_counter_freq(void);   /* counter frequency in Hz */

/* A physical address range of device registers, mapped uncached by the MMU. */
struct hal_mmio_range {
	uptr base;
	usize size;
};

/* The board's device register ranges; returns how many entries *ranges points to. */
int hal_mmio_ranges(const struct hal_mmio_range **ranges);

/* Short, stable board identifier, e.g. "qemu-virt" (shown at boot and in logs). */
const char *hal_board_name(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_HAL_H */
