/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Periodic tick counting and uptime from the board's free-running counter.
 */

#include "hal.h"
#include "irq.h"
#include "thread.h"
#include "time.h"

#define MS_PER_S 1000u

static volatile u64 tick_count;

static void time_tick(u32 irq, void *ctx)
{
	(void)irq;
	(void)ctx;
	tick_count++;
	hal_timer_ack();
	thread_tick();
}

void time_init(void)
{
	irq_register(hal_timer_irq(), time_tick, NULL);
	hal_timer_start(TIME_TICK_HZ);
}

u64 time_ticks(void)
{
	return tick_count;
}

u64 time_uptime_ms(void)
{
	u64 count = hal_counter();
	u64 freq = hal_counter_freq();

	/* Split the division so count * 1000 cannot overflow. */
	return (count / freq) * MS_PER_S + (count % freq) * MS_PER_S / freq;
}
