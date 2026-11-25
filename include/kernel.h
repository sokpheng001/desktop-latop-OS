/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel-wide declarations shared by the boot code and the core kernel.
 */

#ifndef OS001_KERNEL_H
#define OS001_KERNEL_H

#include "compiler.h"
#include "types.h"

#ifndef __ASSEMBLY__

/*
 * First C function, called by arch boot code on the primary core with the MMU off,
 * a valid stack and .bss cleared. dtb is the Device Tree address, or 0 if the loader
 * did not pass one.
 */
void kernel_main(uptr dtb) ATTR_NORETURN;

#endif /* !__ASSEMBLY__ */

#endif /* OS001_KERNEL_H */
