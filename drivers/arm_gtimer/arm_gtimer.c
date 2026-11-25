/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm generic timer: periodic interrupts from the EL1 physical timer, read-out
 * of the system counter. Registers from the Arm Architecture Reference Manual
 * (DDI 0487), chapter D11 "The Generic Timer in AArch64 state", and the
 * CNTFRQ_EL0, CNTPCT_EL0, CNTP_CTL_EL0 and CNTP_CVAL_EL0 descriptions.
 */

#include "drivers/arm_gtimer.h"

/* CNTP_CTL_EL0 bits. */
#define CNTP_CTL_ENABLE (1ul << 0)
#define CNTP_CTL_IMASK	(1ul << 1) /* interrupt masked when set */

static u64 period_ticks;
static u64 next_deadline;

u64 arm_gtimer_freq(void)
{
	u64 f;

	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	return f;
}

u64 arm_gtimer_count(void)
{
	u64 c;

	/* isb: the counter read must not be performed early, out of program order. */
	__asm__ volatile("isb\n\tmrs %0, cntpct_el0" : "=r"(c)::"memory");
	return c;
}

static void set_deadline(u64 cval)
{
	__asm__ volatile("msr cntp_cval_el0, %0\n\tisb" : : "r"(cval) : "memory");
}

void arm_gtimer_start(u32 hz)
{
	period_ticks = arm_gtimer_freq() / hz;
	next_deadline = arm_gtimer_count() + period_ticks;
	set_deadline(next_deadline);
	__asm__ volatile("msr cntp_ctl_el0, %0\n\tisb" : : "r"(CNTP_CTL_ENABLE) : "memory");
}

void arm_gtimer_rearm(void)
{
	/*
	 * Deadlines advance by exactly one period from the previous deadline, not from
	 * "now", so handler latency does not make the tick drift. If ticks were missed
	 * (e.g. interrupts masked for long), restart from now instead of firing a burst.
	 */
	u64 now = arm_gtimer_count();

	next_deadline += period_ticks;
	if (next_deadline <= now)
		next_deadline = now + period_ticks;
	set_deadline(next_deadline);
}
