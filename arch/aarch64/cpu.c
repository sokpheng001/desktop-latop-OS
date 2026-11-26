/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * AArch64 implementation of the architecture interface (include/arch.h).
 */

#include "arch.h"
#include "arch/aarch64/sysregs.h"
#include "types.h"

unsigned int arch_current_el(void)
{
	u64 el;

	__asm__ volatile("mrs %0, CurrentEL" : "=r"(el));
	return (unsigned int)((el >> CURRENTEL_EL_SHIFT) & CURRENTEL_EL_MASK);
}

void arch_halt(void)
{
	/* Mask all interrupts so nothing wakes this core into code that expects a live system. */
	__asm__ volatile("msr daifset, #0xf" ::: "memory");
	for (;;)
		__asm__ volatile("wfe");
}

void arch_irq_enable(void)
{
	__asm__ volatile("msr daifclr, #2" ::: "memory"); /* clear PSTATE.I */
}

void arch_irq_disable(void)
{
	__asm__ volatile("msr daifset, #2" ::: "memory");
}

unsigned long arch_irq_save(void)
{
	u64 daif;

	__asm__ volatile("mrs %0, daif\n"
			 "msr daifset, #2"
			 : "=r"(daif)::"memory");
	return (unsigned long)daif;
}

void arch_irq_restore(unsigned long flags)
{
	__asm__ volatile("msr daif, %0" ::"r"((u64)flags) : "memory");
}

void arch_wait_for_interrupt(void)
{
	__asm__ volatile("wfi" ::: "memory");
}
