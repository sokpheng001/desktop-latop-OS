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
