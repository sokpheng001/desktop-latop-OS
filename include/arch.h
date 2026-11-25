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

/* Install the exception vectors. Call first in kernel_main. */
void arch_exception_init(void);

/* Stop this core for good, with interrupts masked. */
void arch_halt(void) ATTR_NORETURN;

/* Exception (privilege) level the kernel runs at: 1 for a normal kernel on AArch64. */
unsigned int arch_current_el(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_ARCH_H */
