/*
 * Project: OS-001
 * Date: 2026-11-25
 * Authors: kwan, oda, sokpheng001
 *
 * Arm Generic Interrupt Controller v2 driver interface (GIC-400 and QEMU's GICv2).
 */

#ifndef OS001_DRIVERS_GICV2_H
#define OS001_DRIVERS_GICV2_H

#include "types.h"

#ifndef __ASSEMBLY__

/* Interrupt ID the CPU interface returns when nothing is pending (IHI 0048B, "Special interrupt
 * numbers"). */
#define GICV2_SPURIOUS_ID 1023u

struct gicv2 {
	uptr dist_base; /* Distributor (GICD_*) registers */
	uptr cpu_base;	/* CPU interface (GICC_*) registers */
};

/* Reset the distributor and this core's CPU interface; every interrupt starts disabled. */
void gicv2_init(const struct gicv2 *gic);
u32 gicv2_num_irqs(const struct gicv2 *gic);
void gicv2_enable(const struct gicv2 *gic, u32 irq);
void gicv2_disable(const struct gicv2 *gic, u32 irq);

/* Acknowledge the highest-priority pending interrupt; returns the raw GICC_IAR value. */
u32 gicv2_ack(const struct gicv2 *gic);
u32 gicv2_iar_irq(u32 iar);
/* Signal end of interrupt; iar must be the value gicv2_ack returned. */
void gicv2_eoi(const struct gicv2 *gic, u32 iar);

/* Send software-generated interrupt sgi (0-15) to this core only. */
void gicv2_send_sgi_self(const struct gicv2 *gic, u32 sgi);

#endif /* !__ASSEMBLY__ */

#endif /* OS001_DRIVERS_GICV2_H */
