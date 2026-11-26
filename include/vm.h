/*
 * Project: OS-001
 * Date: 2026-11-26
 * Authors: kwan, oda, sokpheng001
 *
 * Virtual memory: the kernel's address space.
 */

#ifndef OS001_VM_H
#define OS001_VM_H

#ifndef __ASSEMBLY__

/*
 * Map all RAM (cached) and the board's device registers (uncached) at their physical
 * addresses and turn on the MMU. Call after mem_init, which starts the page allocator
 * the page tables come from.
 */
void vm_init(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_VM_H */
