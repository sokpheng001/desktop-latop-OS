/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm generic timer driver interface (EL1 physical timer and system counter).
 */

#ifndef OS001_DRIVERS_ARM_GTIMER_H
#define OS001_DRIVERS_ARM_GTIMER_H

#include "types.h"

#ifndef __ASSEMBLY__

/* System counter frequency in Hz, as set by firmware (CNTFRQ_EL0). */
u64 arm_gtimer_freq(void);
/* Current system counter value (CNTPCT_EL0). */
u64 arm_gtimer_count(void);

/* Start a periodic interrupt at hz; call arm_gtimer_rearm() from its handler. */
void arm_gtimer_start(u32 hz);
/* Schedule the next period and clear the current interrupt condition. */
void arm_gtimer_rearm(void);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_ARM_GTIMER_H */
