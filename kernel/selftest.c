/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel self-tests for paths that cannot be reached from a normal boot, such as
 * faults. tests/boot_test.sh builds each one and checks the console output.
 */

#include "arch.h"
#include "compiler.h"
#include "hal.h"
#include "irq.h"
#include "lib/kprintf.h"
#include "selftest.h"
#include "time.h"

#if defined(KERNEL_TEST_IRQ)
/* Software-generated interrupt used by the test; any of 0-15 would do. */
#define SELFTEST_SGI	   1u
/* WFI wake-ups to wait before giving up (a spurious wake-up is allowed). */
#define SELFTEST_IRQ_TRIES 1000

static volatile u32 irq_seen;

static void selftest_irq_handler(u32 irq, void *ctx)
{
	(void)ctx;
	irq_seen = irq + 1;
}
#endif

#if defined(KERNEL_TEST_TIMER)
/* Ticks to wait for: half a second at 100 Hz. */
#define SELFTEST_TICKS	       50u
/* Allowed difference between tick time and counter time, in percent. */
#define SELFTEST_TOLERANCE_PCT 10u
#endif

void selftest_run(void)
{
#if defined(KERNEL_TEST_FAULT)
	/* Must end in the exception handler's report, never return here. */
	kprintf("Self-test: triggering a breakpoint exception\n");
	TRAP();
#endif
#if defined(KERNEL_TEST_IRQ)
	/* Exercises the GIC, the IRQ vector, dispatch and the return to interrupted code. */
	irq_register(SELFTEST_SGI, selftest_irq_handler, NULL);
	hal_irq_raise_soft(SELFTEST_SGI);
	for (int i = 0; i < SELFTEST_IRQ_TRIES && !irq_seen; i++)
		arch_wait_for_interrupt();
	if (irq_seen == SELFTEST_SGI + 1)
		kprintf("Self-test: IRQ %u received and returned\n", SELFTEST_SGI);
	else
		kprintf("Self-test: IRQ FAILED (seen=%u)\n", irq_seen);
#endif
#if defined(KERNEL_TEST_TIMER)
	/* Ticks must arrive, and at the right rate measured against the hardware counter. */
	u64 start_ticks = time_ticks();
	u64 start_ms = time_uptime_ms();

	while (time_ticks() - start_ticks < SELFTEST_TICKS)
		arch_wait_for_interrupt();

	u64 elapsed_ms = time_uptime_ms() - start_ms;
	u64 expect_ms = SELFTEST_TICKS * 1000u / TIME_TICK_HZ;
	u64 slack_ms = expect_ms * SELFTEST_TOLERANCE_PCT / 100u;

	if (elapsed_ms + slack_ms >= expect_ms && elapsed_ms <= expect_ms + slack_ms)
		kprintf("Self-test: timer OK (%u ticks in %lu ms)\n", SELFTEST_TICKS,
			(unsigned long)elapsed_ms);
	else
		kprintf("Self-test: timer FAILED (%u ticks in %lu ms, expected %lu ms)\n",
			SELFTEST_TICKS, (unsigned long)elapsed_ms, (unsigned long)expect_ms);
#endif
}
