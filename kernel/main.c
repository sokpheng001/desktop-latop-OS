/*
 * Project: OS-001
 * Authors: kwan, oda, sokpheng001
 *
 * kernel_main: architecture-independent kernel entry after early boot.
 */

#include "kernel.h"

void kernel_main(uptr dtb)
{
	/* Device Tree parsing arrives with the memory phase (ROADMAP Phase 3). */
	(void)dtb;
}
