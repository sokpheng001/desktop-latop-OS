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
	ARCH_MEM_NORMAL, /* RAM: cached, read/write, never executed */
	ARCH_MEM_TEXT,	 /* kernel code: cached, read-only, executable */
	ARCH_MEM_RODATA, /* kernel constants: cached, read-only, never executed */
	ARCH_MEM_DEVICE, /* device registers: uncached, strictly ordered, never executed */
};

struct arch_mem_map {
	u64 base;
	u64 size;
	enum arch_mem_type type;
};

/*
 * Build page tables that map each range at its own physical address (identity map),
 * then turn on the MMU and caches. Ranges must not overlap. Nothing else is mapped,
 * so any other access faults; no range is both writable and executable. Returns the number of 4 KB
 * table pages used, or -1 (MMU left off) if a range is outside the virtual address space or out of
 * memory for tables.
 */
int arch_mmu_init(const struct arch_mem_map *maps, int n);

/*
 * Threads. A thread that is not running is fully described by its saved stack pointer.
 * arch_context_init prepares a fresh stack so that the first switch to it calls
 * start(arg) (start must never return) and returns the stack pointer to switch to.
 * arch_context_switch saves the running thread's stack pointer in *save_sp and
 * resumes the thread whose saved stack pointer is new_sp.
 */
uptr arch_context_init(uptr stack_top, void (*start)(void *), void *arg);
void arch_context_switch(uptr *save_sp, uptr new_sp);

/* User (EL0) page permissions. The kernel never executes user pages. */
enum arch_user_perm {
	ARCH_USER_CODE, /* user: read and execute; kernel: read */
	ARCH_USER_DATA, /* user and kernel: read and write; never executed */
};

/*
 * Address spaces. A space is the kernel's mappings plus a process's own user pages,
 * identified by an address space ID (1..ARCH_MAX_ASID) so switching between spaces
 * needs no TLB flush. 0 stands for the kernel's own space (no user pages).
 */
#define ARCH_MAX_ASID 255u

/* A new space with only the kernel mapped, or 0 if out of memory or asid is invalid. */
uptr arch_mmu_space_create(u32 asid);

/* Free a space's own page tables and flush its ASID. Its pages must be unmapped first. */
void arch_mmu_space_destroy(uptr space);

/* Make space (0: the kernel's) the current one on this core. */
void arch_mmu_switch(uptr space);

/* Map one 4 KB page of physical memory at user address va in space (both page-aligned). */
bool arch_mmu_map_user(uptr space, u64 va, u64 pa, enum arch_user_perm perm);

/* Remove the user mapping at va from space and flush it from the TLB. */
void arch_mmu_unmap_user(uptr space, u64 va);

/* Make len bytes of code just written at kernel address addr safe to execute. */
void arch_sync_icache(uptr addr, usize len);

/*
 * Leave the kernel: continue at entry in user mode (EL0) on the user stack sp, with
 * interrupts enabled and arg in the first argument register; every other register is
 * zero. The thread comes back into the kernel only through exceptions.
 */
void arch_enter_user(uptr entry, uptr sp, u64 arg) ATTR_NORETURN;

/* Virtual address bits the MMU translates (the size of the address space). */
unsigned int arch_mmu_va_bits(void);

/* Install the exception vectors. Call first in kernel_main. */
void arch_exception_init(void);

/* Unmask / mask IRQs on this core. */
void arch_irq_enable(void);
void arch_irq_disable(void);

/* Mask IRQs and return the previous mask state, for arch_irq_restore. */
unsigned long arch_irq_save(void);
void arch_irq_restore(unsigned long flags);

/* Sleep until an interrupt is pending (it may be masked). */
void arch_wait_for_interrupt(void);

/* Stop this core for good, with interrupts masked. */
void arch_halt(void) ATTR_NORETURN;

/* Exception (privilege) level the kernel runs at: 1 for a normal kernel on AArch64. */
unsigned int arch_current_el(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_ARCH_H */
