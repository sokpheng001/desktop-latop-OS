/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Kernel time: periodic tick and uptime.
 */

#ifndef OS001_TIME_H
#define OS001_TIME_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Scheduler and timekeeping tick rate. */
#define TIME_TICK_HZ 100u

/* Start the periodic tick. Needs the interrupt controller (hal_init) first. */
void time_init(void);

/* Ticks since time_init(). */
u64 time_ticks(void);

/* Milliseconds since the system counter started (normally power-on), from the hardware counter. */
u64 time_uptime_ms(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_TIME_H */
