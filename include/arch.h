/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Architecture interface: CPU-level queries and operations the core kernel may use
 * without knowing the architecture. Implemented in arch/<arch>/.
 */

#ifndef OS001_ARCH_H
#define OS001_ARCH_H

#ifndef __ASSEMBLY__

#include "compiler.h"
#include "types.h"

/* Memory types the MMU can map a range as. */
enum arch_mem_type {
	ARCH_MEM_NORMAL, /* RAM: cached, may be reordered */
	ARCH_MEM_DEVICE, /* device registers: uncached, strictly ordered, never executed */
};

struct arch_mem_map {
	u64 base;
	u64 size;
	enum arch_mem_type type;
};

/*
 * Build page tables that map each range at its own physical address (identity map),
 * then turn on the MMU and caches. Nothing else is mapped, so any other access
 * faults. Returns the number of 4 KB table pages used, or -1 (MMU left off) if a
 * range is outside the virtual address space or out of memory for tables.
 */
int arch_mmu_init(const struct arch_mem_map *maps, int n);

/* Virtual address bits the MMU translates (the size of the address space). */
unsigned int arch_mmu_va_bits(void);

/* Install the exception vectors. Call first in kernel_main. */
void arch_exception_init(void);

/* Unmask / mask IRQs on this core. */
void arch_irq_enable(void);
void arch_irq_disable(void);

/* Sleep until an interrupt is pending (it may be masked). */
void arch_wait_for_interrupt(void);

/* Stop this core for good, with interrupts masked. */
void arch_halt(void) ATTR_NORETURN;

/* Exception (privilege) level the kernel runs at: 1 for a normal kernel on AArch64. */
unsigned int arch_current_el(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_ARCH_H */
