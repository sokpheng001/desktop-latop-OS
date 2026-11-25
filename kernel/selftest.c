/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel self-tests for paths that cannot be reached from a normal boot, such as
 * faults. tests/boot_test.sh builds each one and checks the console output.
 */

#include "compiler.h"
#include "lib/kprintf.h"
#include "selftest.h"

void selftest_run(void)
{
#if defined(KERNEL_TEST_FAULT)
	/* Must end in the exception handler's report, never return here. */
	kprintf("Self-test: triggering a breakpoint exception\n");
	TRAP();
#endif
}
