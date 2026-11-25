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

/* Exception (privilege) level the kernel runs at: 1 for a normal kernel on AArch64. */
unsigned int arch_current_el(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_ARCH_H */
