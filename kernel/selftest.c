/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel self-tests for paths that cannot be reached from a normal boot, such as
 * faults. tests/boot_test.sh builds each one and checks the console output
 * (see tests/README.md for the full list).
 */

#include "arch.h"
#include "compiler.h"
#include "hal.h"
#include "irq.h"
#include "lib/kprintf.h"
#include "selftest.h"
#include "time.h"

#if defined(KERNEL_TEST_IRQ)
/* Software-generated interrupts used by the test; any of 0-15 would do. */
#define SELFTEST_SGI	       1u
/*
 * SGIs may be impossible to disable (GICv2 leaves that IMPLEMENTATION DEFINED, and
 * QEMU keeps them enabled), so the no-handler case uses the highest shared
 * interrupt, set pending from software. No device on either board uses it.
 */
#define SELFTEST_SPI_NOHANDLER (hal_irq_count() - 1)
/* Timer ticks to wait for an interrupt that must not arrive. */
#define SELFTEST_QUIET_TICKS   5u
/* WFI wake-ups to wait before giving up (a spurious wake-up is allowed). */
#define SELFTEST_IRQ_TRIES     1000

static volatile u32 irq_seen;

static void selftest_irq_handler(u32 irq, void *ctx)
{
	(void)ctx;
	irq_seen = irq + 1;
}
#endif

#if defined(KERNEL_TEST_ABORT)
static u8 abort_buf[8] ATTR_ALIGNED(8);
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
#if defined(KERNEL_TEST_ABORT)
	/*
	 * With the MMU off all memory is Device memory, where a misaligned load always
	 * faults: this must end in a data abort report whose FAR is this address.
	 */
	uptr addr = (uptr)abort_buf + 1;

	/* Otherwise -mstrict-align lets the compiler split the known-misaligned load into bytes. */
	HIDE_VALUE(addr);
	volatile u32 *misaligned = (volatile u32 *)addr;

	kprintf("Self-test: misaligned load from %p\n", (void *)misaligned);
	(void)*misaligned;
	kprintf("Self-test: abort FAILED (the load did not fault)\n");
#endif
#if defined(KERNEL_TEST_IRQ)
	/* Exercises the GIC, the IRQ vector, dispatch and the return to interrupted code. */
	irq_register(SELFTEST_SGI, selftest_irq_handler, NULL);
	hal_irq_raise(SELFTEST_SGI);
	for (int i = 0; i < SELFTEST_IRQ_TRIES && !irq_seen; i++)
		arch_wait_for_interrupt();
	if (irq_seen == SELFTEST_SGI + 1)
		kprintf("Self-test: IRQ %u received and returned\n", SELFTEST_SGI);
	else
		kprintf("Self-test: IRQ FAILED (seen=%u)\n", irq_seen);

	/* An interrupt with no handler is reported and disabled, so raising it again is silent. */
	u32 spi = SELFTEST_SPI_NOHANDLER;

	hal_irq_enable(spi);
	hal_irq_raise(spi);
	u64 t0 = time_ticks();
	while (time_ticks() - t0 < SELFTEST_QUIET_TICKS)
		arch_wait_for_interrupt();
	hal_irq_raise(spi);
	t0 = time_ticks();
	while (time_ticks() - t0 < SELFTEST_QUIET_TICKS)
		arch_wait_for_interrupt();
	if (irq_unhandled_count() == 1)
		kprintf("Self-test: unhandled IRQ %u reported once and disabled\n", spi);
	else
		kprintf("Self-test: unhandled IRQ FAILED (count=%lu)\n",
			(unsigned long)irq_unhandled_count());
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
